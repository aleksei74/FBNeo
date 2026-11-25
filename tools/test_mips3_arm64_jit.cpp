// Standalone differential test: exercise the real MIPS3 core under AArch64.
// See tools/test_mips3_arm64_jit.md for build/run instructions.
#include "../src/cpu/mips3/mips3.cpp"

#if !MIPS3_ARM64_JIT
#error This test must be built for Linux AArch64 with the experimental JIT enabled.
#endif

#include <vector>

// Libretro redirects file I/O to VFS; test diagnostics use the host stderr.
#ifdef fprintf
#undef fprintf
#endif

static INT32 __cdecl NoScan(BurnArea *) { return 0; }
INT32 (__cdecl *BurnAcb)(BurnArea *) = NoScan;
void CpuCheatRegister(INT32, cpu_core_config *) {}
#ifndef bprintf
static INT32 __cdecl NoLog(INT32, TCHAR *, ...) { return 0; }
INT32 (__cdecl *bprintf)(INT32, TCHAR *, ...) = NoLog;
#endif

static void Require(bool ok, const char *message)
{
	if (!ok) { fprintf(stderr, "FAIL: %s\n", message); exit(1); }
}

static uint64_t rng = 0x3401c0ffee123456ULL;
static uint64_t Random()
{
	rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
	return rng;
}

static uint32_t R(unsigned rs, unsigned rt, unsigned rd, unsigned sa, unsigned fn)
{
	return (rs << 21) | (rt << 16) | (rd << 11) | (sa << 6) | fn;
}

static uint32_t Imm(unsigned opc, unsigned rs, unsigned rt, int imm)
{
	return (opc << 26) | (rs << 21) | (rt << 16) | uint16_t(imm);
}

static uint32_t RandomOp(unsigned kind)
{
	static const unsigned immediate[] = { 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x19 };
	static const unsigned arithmetic[] = { 0x21, 0x23, 0x24, 0x25, 0x26, 0x27, 0x2a, 0x2b, 0x2d, 0x2f };
	static const unsigned shift[] = { 0x00, 0x02, 0x03, 0x38, 0x3a, 0x3b, 0x3c, 0x3e, 0x3f };
	static const unsigned variable[] = { 0x04, 0x06, 0x07, 0x14, 0x16, 0x17 };
	unsigned rs = Random() & 31, rt = Random() & 31, rd = Random() & 31, sa = Random() & 31;
	if (kind < 8) return Imm(immediate[kind], kind == 6 ? 0 : rs, rt, Random());
	if (kind < 18) return R(rs, rt, rd, 0, arithmetic[kind - 8]);
	if (kind < 27) return R(0, rt, rd, sa, shift[kind - 18]);
	if (kind >= 33) return R(rs, rt, rd, 0, kind == 33 ? 0x0a : 0x0b);
	return R(rs, rt, rd, 0, variable[kind - 27]);
}

static void TestEmitter(int type)
{
	nMips3Type = type;
	Mips3Arm64Jit::Cache cache;
	const unsigned cases = 2048, length = 66;
	for (unsigned c = 0; c < cases; c++) {
		if (!(c % 128)) cache.Reset();
		uint64_t regs[32];
		for (unsigned i = 0; i < 32; i++) M.r[i] = Random();
		M.r[0] = 0;
		M.r[1] = 0x80000000ULL; M.r[2] = 0x8000000000000000ULL;
		M.r[3] = ~0ULL; M.r[4] = 0x7fffffffULL;
		memcpy(regs, M.r, sizeof(regs));
		uint32_t words[length * Mips3Arm64Jit::MaxWordsPerInsn + 2];
		unsigned used = 0;
		words[used++] = Mips3Arm64Jit::Entry;
		for (unsigned i = 0; i < length; i++) {
			Mips3Insn I = {};
			I.op = RandomOp((i + c) % 35);
			if ((I.op >> 26) == 0 && ((I.op & 63) == 0x0a || (I.op & 63) == 0x0b) && (c & 1))
				I.op &= ~(31u << 16); // force zero condition
			UINT32 flags = 0;
			I.fn = Mips3Decode(I.op, &flags);
			InsnFields(&I);
			Require(flags == 0, "test instruction unexpectedly changes flow");
			int n = Mips3Arm64Jit::EmitAdjacent(I.op, words + used, used > 1);
			Require(n >= 0 && n <= int(Mips3Arm64Jit::MaxWordsPerInsn), "emitter rejected supported instruction");
			used += n;
			I.fn(&I);
		}
		words[used++] = Mips3Arm64Jit::Ret;
		uint32_t offset = 0;
		Require(cache.Publish(words, used, offset), "RX publication failed");
		cache.Run(offset, regs);
		if (memcmp(M.r, regs, sizeof(regs))) {
			fprintf(stderr, "type %d case %u: GPR mismatch\n", type, c);
			exit(1);
		}
	}
	uint32_t scratch[8];
	Require(Mips3Arm64Jit::Emit(Imm(8, 1, 2, -1), scratch) < 0, "trapping ADDI must fallback");
	Require(Mips3Arm64Jit::Emit(Imm(0x23, 1, 2, 0), scratch) < 0, "LW must fallback");
	Require(Mips3Arm64Jit::Emit(Imm(4, 1, 2, 1), scratch) < 0, "branch must fallback");
	Require(Mips3Arm64Jit::Emit(R(1, 2, 3, 7, 2), scratch) < 0, "VR5500 rotate must fallback");
	printf("Emitter type %d: %u native sequences / %u guest instructions OK\n", type, cases, cases * length);
}

struct Snapshot {
	Mips3State state;
	UINT32 data;
	INT32 cycleDebt;
};

static std::vector<Snapshot> RunProgram(int type, bool enable)
{
	std::vector<Snapshot> result;
	UINT32 *ram = (UINT32 *)calloc(0x2000, 1);
	Require(ram != NULL, "allocate synthetic RAM");
	const uint32_t code[] = {
		Imm(9, 0, 1, 37), Imm(0xf, 0, 2, 0x8000), Imm(0xd, 2, 2, 0x1000), Imm(9, 0, 3, 5),
		// Long initial span exercises configurable thresholds up to eight.
		Imm(9, 0, 11, 7), Imm(9, 11, 12, 1), Imm(9, 12, 13, 1), Imm(9, 13, 14, 1),
		// Loop: interpreted load/store and branch around a native ALU span.
		Imm(0x23, 2, 4, 0), R(4, 3, 4, 0, 0x21), Imm(0xe, 4, 4, 0x1234), R(0, 4, 5, 3, 0), Imm(0x19, 5, 6, -3),
		Imm(0x2b, 2, 6, 0), Imm(9, 1, 1, -1), Imm(5, 1, 0, -8), Imm(9, 3, 3, 1),
		// Likely branch, not taken: its delay slot must be annulled.
		Imm(0x14, 0, 3, 1), Imm(9, 7, 7, 77),
		Imm(9, 0, 8, -1), Imm(0xc, 8, 9, 0x8000), R(8, 9, 10, 0, 0x27),
		// Spin at the end, allowing the existing idle optimizer to run.
		0x08000016, 0
	};
	for (unsigned i = 0; i < sizeof(code)/sizeof(code[0]); i++) ram[i] = BURN_ENDIAN_SWAP_INT32(code[i]);
	Mips3Init(type);
	Require(Mips3UseRecompiler(enable) == 0, "set experimental backend");
	Mips3MapMemory((UINT8 *)ram, 0, 0x1fff, MAP_RAM);
	Mips3Reset();
	M.pc = 0x80000000;
	for (unsigned stage = 0; stage < 3; stage++) {
		for (unsigned i = 0; i < 31; i++) {
			Mips3Run(17 + i % 13);
			Snapshot snap = {};
			snap.state = M;
			snap.data = ram[0x1000/4];
			snap.cycleDebt = nMips3ICount;
			result.push_back(snap);
		}
		Require(M.r[7] == 0 && M.r[0] == 0, "annulled delay or zero register corrupted");
		if (stage == 0) {
			// Patch the first instruction in a cached native span, invalidate
			// that physical range and run again without resetting the cache.
			ram[0] = BURN_ENDIAN_SWAP_INT32(Imm(9, 0, 1, 29));
			Mips3InvalidateRange(0, 3);
		} else if (stage == 1) {
			Mips3Scan(ACB_WRITE); // restore path flushes decoded and native caches
		}
		M.pc = 0x80000000;
	}
	if (type == MIPS3_TYPE_VR5500) {
		Mips3PerfStats stats;
		Mips3GetPerfStats(&stats);
		Require(stats.guestInstructions > 0, "missing guest instruction counters");
		Require(stats.nativeInstructions <= stats.guestInstructions, "invalid native instruction count");
		Require(stats.jitEnabled == enable, "wrong reported backend");
		Require((stats.nativeInstructions > 0) == enable, "incorrect native work counter");
		Require(stats.cacheFlushes > 0, "missing cache flush counter");
	}
	if (enable) {
		Require(nJitSpansBuilt > 0 && nJitSpansRun > 0 && nJitInsns > 0, "no native span executed");
		printf("Core type %d: %llu native spans run, %llu guest instructions\n", type,
			(unsigned long long)nJitSpansRun, (unsigned long long)nJitInsns);
	}
	Mips3Exit();
	free(ram);
	return result;
}

int main()
{
	// Verify default policy and explicit process-environment overrides.
	for (int type = 0; type < 2; type++) {
		unsetenv("FBNEO_MIPS3_ARM64_JIT");
		Mips3Init(type);
		Require(bMips3Jit == (type == MIPS3_TYPE_VR5500), "wrong default backend");
		Mips3Exit();
		for (int enabled = 0; enabled < 2; enabled++) {
			setenv("FBNEO_MIPS3_ARM64_JIT", enabled ? "1" : "0", 1);
			Mips3Init(type);
			Require(bMips3Jit == bool(enabled), "environment override failed");
			Mips3Exit();
		}
	}
	unsetenv("FBNEO_MIPS3_ARM64_JIT");
	for (int type = 0; type < 2; type++) {
		TestEmitter(type);
		std::vector<Snapshot> a = RunProgram(type, false), b = RunProgram(type, true);
		Require(a.size() == b.size(), "snapshot count differs");
		for (size_t i = 0; i < a.size(); i++) {
			Require(!memcmp(&a[i].state, &b[i].state, sizeof(Mips3State)), "CPU state differs");
			Require(a[i].data == b[i].data && a[i].cycleDebt == b[i].cycleDebt, "memory or cycle debt differs");
		}
		printf("Core type %d: %zu interpreter/JIT snapshots identical\n", type, a.size());
	}
	puts("PASS: ARM64 native ALU + mixed-block differential tests");
	return 0;
}
