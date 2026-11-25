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

// Multiple independent spans can now share a host page. Verify that
// rewriting its permissions to append code does not damage earlier spans.
static void TestPackedCache()
{
	const size_t page = size_t(sysconf(_SC_PAGESIZE));
	Mips3Arm64Jit::Cache packed(page * 2, true), legacy(page * 2, false);
	uint32_t offsets[3], oldOffsets[2], off = 0;
	uint64_t regs[32] = {};
	for (unsigned i = 0; i < 3; i++) {
		uint32_t words[] = {
			Mips3Arm64Jit::Entry,
			Mips3Arm64Jit::Immediate(1, uint16_t(i + 42), false),
			Mips3Arm64Jit::Store(5),
			Mips3Arm64Jit::Ret
		};
		Require(packed.Publish(words, 4, offsets[i]), "packed cache publication failed");
		Require(offsets[i] == i * 16, "packed offsets not code-aligned");
		if (i < 2) {
			Require(legacy.Publish(words, 4, oldOffsets[i]), "legacy cache publication failed");
			Require(oldOffsets[i] == i * page, "legacy offsets not page-aligned");
		} else {
			Require(!legacy.Publish(words, 4, off) && !legacy.Denied(), "legacy capacity not enforced");
		}
		for (unsigned j = 0; j <= i; j++) {
			packed.Run(offsets[j], regs);
			Require(regs[5] == j + 42, "earlier executable span changed after appending");
		}
	}
	Require(packed.Used() == 48 && packed.Payload() == 48 &&
		packed.MappedCodeBytes() == page, "wrong packed cache accounting");
	Require(legacy.Used() == 2 * page && legacy.Payload() == 32, "wrong legacy cache accounting");
	// Append a span that crosses into the next page, then fill to capacity.
	std::vector<uint32_t> large(page / 4 + 5, 0xd503201fu); // NOP
	large.front() = Mips3Arm64Jit::Entry;
	large.back() = Mips3Arm64Jit::Ret;
	Require(packed.Publish(large.data(), large.size(), off), "cross-page span failed");
	Require(off == 48 && packed.MappedCodeBytes() == 2 * page, "cross-page accounting differs");
	packed.Run(off, regs);
	for (unsigned j = 0; j < 3; j++) {
		packed.Run(offsets[j], regs);
		Require(regs[5] == j + 42, "cross-page append damaged old code");
	}
	std::vector<uint32_t> tail((packed.CapacityBytes() - packed.Used()) / 4, 0xd503201fu);
	Require(tail.size() >= 2, "unexpected test cache geometry");
	tail.front() = Mips3Arm64Jit::Entry;
	tail.back() = Mips3Arm64Jit::Ret;
	Require(packed.Publish(tail.data(), tail.size(), off), "exact capacity append failed");
	Require(packed.Used() == packed.CapacityBytes(), "cache capacity mismatch");
	packed.Run(off, regs);
	Require(!packed.Publish(large.data(), large.size(), off) && !packed.Denied(), "full cache not rejected");
	packed.Run(offsets[0], regs);
	Require(regs[5] == 42, "capacity rejection damaged cached code");
	packed.Reset();
	Require(packed.Used() == 0 && packed.Payload() == 0 && packed.MappedCodeBytes() == 0, "cache reset accounting differs");
	uint32_t words[] = {Mips3Arm64Jit::Entry, Mips3Arm64Jit::Ret};
	Require(packed.Publish(words, 2, off) && off == 0, "cache cannot reuse after reset");
	packed.Run(off, regs);
	printf("Packed W^X cache: shared-page, cross-page, capacity and reset OK (host page %zu bytes)\n", page);
}

static void TestEmitter(int type)
{
	nMips3Type = type;
	Mips3Arm64Jit::Cache cache;
	const unsigned cases = 2048, length = 66;
	for (unsigned c = 0; c < cases; c++) {
		if (!(c % 128)) cache.Reset();
		uint64_t regs[32], integrated[32];
		for (unsigned i = 0; i < 32; i++) M.r[i] = Random();
		M.r[0] = 0;
		M.r[1] = 0x80000000ULL; M.r[2] = 0x8000000000000000ULL;
		M.r[3] = ~0ULL; M.r[4] = 0x7fffffffULL;
		memcpy(regs, M.r, sizeof(regs));
		memcpy(integrated, M.r, sizeof(integrated));
		uint32_t optWords[length * Mips3Arm64Jit::IntegratedMaxWords + 2];
		optWords[0] = Mips3Arm64Jit::Entry;
		Mips3Arm64Jit::SpanEmitter emitter(optWords + 1);
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
			Require(emitter.Alu(I.op), "register-cache ALU rejected supported instruction");
			I.fn(&I);
		}
		emitter.Finish(length, 0, false);
		words[used++] = Mips3Arm64Jit::Ret;
		uint32_t offset = 0;
		Require(cache.Publish(words, used, offset), "RX publication failed");
		cache.Run(offset, regs);
		Require(cache.Publish(optWords, emitter.End() - optWords, offset), "publish cached-register span");
		cache.Run(offset, integrated);
		if (memcmp(M.r, regs, sizeof(regs)) || memcmp(M.r, integrated, sizeof(integrated))) {
			fprintf(stderr, "type %d case %u: GPR mismatch\n", type, c);
			exit(1);
		}
	}
	uint32_t scratch[8];
	Require(Mips3Arm64Jit::Emit(Imm(8, 1, 2, -1), scratch) < 0, "trapping ADDI must fallback");
	Require(Mips3Arm64Jit::Emit(Imm(0x23, 1, 2, 0), scratch) < 0, "LW must fallback");
	Require(Mips3Arm64Jit::Emit(Imm(4, 1, 2, 1), scratch) < 0, "branch must fallback");
	Require(Mips3Arm64Jit::Emit(R(1, 2, 3, 7, 2), scratch) < 0, "VR5500 rotate must fallback");
	// Adjacent emission forwards x1 and removes stores proven unobservable.
	uint32_t pair[16];
	pair[0] = Mips3Arm64Jit::Entry;
	int a = Mips3Arm64Jit::Emit(R(4, 5, 6, 0, 0x21), pair + 1);
	int b = Mips3Arm64Jit::EmitAdjacent(R(6, 7, 6, 0, 0x21), pair + 1 + a, true);
	Require(b == a - 2, "dependent overwrite must forward x1 and remove both words");
	Require(pair[1 + a] != Mips3Arm64Jit::Load(1, 6), "dependent adjacent result was not forwarded");
	Require(pair[a] == Mips3Arm64Jit::Load(2, 7), "self-overwrite must remove the forwarded and dead-store words");
	// MOVZ preserves the old destination when its condition is zero.
	pair[0] = Mips3Arm64Jit::Entry;
	a = Mips3Arm64Jit::Emit(Imm(9, 1, 9, 5), pair + 1);
	b = Mips3Arm64Jit::EmitAdjacent(R(4, 5, 9, 0, 0x0a), pair + 1 + a, true);
	Require(pair[a] == Mips3Arm64Jit::Store(9), "MOVZ must retain its preserved destination");
	pair[0] = Mips3Arm64Jit::Entry;
	a = Mips3Arm64Jit::Emit(R(1, 2, 3, 0, 0x21), pair + 1);
	b = Mips3Arm64Jit::EmitAdjacent(Imm(9, 4, 3, 1), pair + 1 + a, true);
	Require(b == a - 1, "overwritten destination store must be eliminated");
	pair[0] = Mips3Arm64Jit::Entry;
	a = Mips3Arm64Jit::Emit(R(1, 2, 3, 0, 0x21), pair + 1);
	b = Mips3Arm64Jit::EmitAdjacent(R(3, 5, 6, 0, 0x21), pair + 1 + a, true);
	Require(b == a - 1 && pair[a] == Mips3Arm64Jit::Store(3) && pair[1 + a] == Mips3Arm64Jit::Load(2, 5), "destination read as a source must retain the store and forward the load");
	printf("Emitter type %d: %u native sequences / %u guest instructions OK\n", type, cases, cases * length);
}

// Compare generated memory instructions against the existing leaf handlers.
// A host-cache miss must exit before changing any GPR or host memory.
static void TestGuardedMemoryEmitter(int type)
{
	nMips3Type = type;
	static const unsigned ops[] = {0x20,0x24,0x21,0x25,0x23,0x27,0x37,0x28,0x29,0x2b,0x3f};
	static const unsigned sizes[] = {1,1,2,2,4,4,8,1,2,4,8};
	static const int offsets[] = {0,1,-1,4095,-4095,4096,-4096,32767,-32768};
	Mips3Arm64Jit::Cache cache;
	alignas(8) UINT8 ref[4096], actual[4096];
	Mips3HostEntry read[MIPS3_HOST_ENTRIES] = {}, write[MIPS3_HOST_ENTRIES] = {};
	for (unsigned c = 0; c < 2048; c++) {
		if (!(c % 128)) cache.Reset();
		unsigned kind = c % 11, size = sizes[kind];
		bool store = kind >= 7;
		unsigned rs = 1 + Random() % 31, rt = store ? Random() % 32 : 1 + Random() % 31;
		int imm = offsets[(c / 11) % 9];
		UINT32 va = (UINT32(Random()) & ~0xfffU) | ((UINT32(Random()) & 0xfffU) & ~(size - 1));
		UINT32 page = va & ~0xfffU, idx = (va >> 12) & (MIPS3_HOST_ENTRIES - 1);
		for (unsigned i = 0; i < 32; i++) M.r[i] = Random();
		M.r[0] = 0;
		M.r[rs] = (Random() & 0xffffffff00000000ULL) | UINT32(va - UINT32(imm));
		for (unsigned i = 0; i < sizeof(ref); i++) ref[i] = UINT8(Random());
		memcpy(actual, ref, sizeof(ref));
		uint64_t regs[32];
		memcpy(regs, M.r, sizeof(regs));
		const uint64_t originalBase = regs[rs];
		Mips3Insn I = {};
		I.op = Imm(ops[kind], rs, rt, imm);
		UINT32 flags = 0;
		I.fn = Mips3Decode(I.op, &flags);
		InsnFields(&I);
		Require(flags == 0, "memory test instruction unexpectedly changes flow");
		HOST_R(va)->tag = HOST_W(va)->tag = page;
		HOST_R(va)->host = HOST_W(va)->host = ref;
		I.fn(&I);
		read[idx].tag = write[idx].tag = page;
		read[idx].host = write[idx].host = actual;
		uint32_t words[32], offset = 0;
		unsigned used = 0;
		words[used++] = Mips3Arm64Jit::Entry;
		int n = Mips3Arm64Jit::EmitMemory(I.op, words + used, 0, 0);
		Require(n > 0 && n <= int(Mips3Arm64Jit::MaxMemoryWords), "memory emitter failed");
		used += n;
		used += Mips3Arm64Jit::EmitReturn(words + used, 1, 1);
		Require(cache.Publish(words, used, offset), "publish memory span");
		Require(cache.RunGuarded(offset, regs, read, write) == 0x10001, "aligned hit failed");
		Require(!memcmp(regs, M.r, sizeof(regs)) && !memcmp(ref, actual, sizeof(ref)), "native RAM result differs");
		// Wrong tag, including a page-cache collision, must side exit.
		read[idx].tag = write[idx].tag = page ^ 0x200000;
		regs[rs] = originalBase;
		uint64_t before[32];
		memcpy(before, regs, sizeof(before));
		Require(cache.RunGuarded(offset, regs, read, write) == 0, "wrong-tag access did not exit");
		Require(!memcmp(before, regs, sizeof(regs)) && !memcmp(ref, actual, sizeof(ref)), "guard exit changed state");
		// Alignment guard (same page, unaligned effective address).
		if (size > 1 && rs != rt) {
			read[idx].tag = write[idx].tag = page;
			regs[rs] += 1;
			memcpy(before, regs, sizeof(before));
			Require(cache.RunGuarded(offset, regs, read, write) == 0, "unaligned access did not exit");
			Require(!memcmp(before, regs, sizeof(regs)) && !memcmp(ref, actual, sizeof(ref)), "unaligned exit changed state");
		}
	}
	HostCacheFlush();
	printf("Guarded RAM type %d: 2048 load/store comparisons and side exits OK\n", type);
}

// Compare mixed register-cache/guarded RAM spans with the exact prefix
// executed by the interpreter, including a miss at each memory position.
static void TestIntegratedMemory(int type)
{
	nMips3Type = type;
	Mips3Arm64Jit::Cache cache;
	alignas(8) UINT8 ref[4096], actual[4096];
	Mips3HostEntry read[MIPS3_HOST_ENTRIES] = {}, write[MIPS3_HOST_ENTRIES] = {};
	const unsigned ops[] = {0x20,0x24,0x21,0x25,0x23,0x27,0x37,0x28,0x29,0x2b,0x3f};
	for (unsigned c = 0; c < 1024; c++) {
		if (!(c % 128)) cache.Reset();
		for (unsigned i = 0; i < 32; i++) M.r[i] = Random();
		M.r[0] = 0;
		M.r[1] = 0xffffffff80001000ULL;
		M.r[2] = 0xffffffff80002000ULL;
		uint64_t regs[32];
		memcpy(regs, M.r, sizeof(regs));
		for (unsigned i = 0; i < sizeof(ref); i++) ref[i] = UINT8(Random());
		memcpy(actual, ref, sizeof(ref));
		const UINT32 va = 0x80001000, idx = (va >> 12) & (MIPS3_HOST_ENTRIES - 1);
		HOST_R(va)->tag = HOST_W(va)->tag = va;
		HOST_R(va)->host = HOST_W(va)->host = ref;
		read[idx].tag = write[idx].tag = va;
		read[idx].host = write[idx].host = actual;
		const unsigned miss = c % 12;
		uint32_t words[32 * Mips3Arm64Jit::IntegratedMaxWords + 16], offset = 0;
		words[0] = Mips3Arm64Jit::Entry;
		Mips3Arm64Jit::SpanEmitter emitter(words + 1);
		unsigned done = 0, memory = 0, prefix = 0, hits = 0;
		bool stopped = false;
		for (unsigned i = 0; i < 11; i++) {
			Mips3Insn I = {};
			unsigned r = 3 + Random() % 29;
			I.op = Imm(9, r, r, int16_t(Random()));
			UINT32 flags = 0;
			I.fn = Mips3Decode(I.op, &flags);
			InsnFields(&I);
			Require(emitter.Alu(I.op), "mixed ALU emit failed");
			if (!stopped) { I.fn(&I); prefix++; }
			done++;
			r = 3 + Random() % 29;
			I.op = Imm(ops[(i + c) % 11], i == miss ? 2 : 1, r, (i * 8) & 0xff8);
			I.fn = Mips3Decode(I.op, &flags);
			InsnFields(&I);
			Require(emitter.Memory(I.op, done, memory), "mixed memory emit failed");
			if (i == miss) stopped = true;
			if (!stopped) { I.fn(&I); prefix++; hits++; }
			done++; memory++;
		}
		emitter.Finish(done, memory, false);
		Require(cache.Publish(words, emitter.End() - words, offset), "publish integrated memory");
		Require(cache.RunGuarded(offset, regs, read, write) == (prefix | (hits << 16)), "mixed side-exit count differs");
		Require(!memcmp(regs, M.r, sizeof(regs)) && !memcmp(ref, actual, sizeof(ref)), "mixed dirty GPR or memory state differs");
	}
	HostCacheFlush();
	printf("Integrated memory type %d: 1024 mixed spans and dirty-register side exits OK\n", type);
}

static void TestNativeBranches(int type)
{
	nMips3Type = type;
	Mips3Arm64Jit::Cache cache;
	const unsigned opc[] = {4,5,6,7,1,1,1,1,2,3,0,0,0x14,0x15,0x16,0x17,1,1,1,1};
	for (unsigned c = 0; c < 4096; c++) {
		if (!(c % 128)) cache.Reset();
		for (unsigned i = 0; i < 32; i++) M.r[i] = Random();
		M.r[0] = 0;
		unsigned k = c % 20, rs = Random() & 31, rt = Random() & 31, rd = Random() & 31;
		if ((c / 12) % 4 == 0) M.r[rs] = 0;
		if ((c / 12) % 4 == 1) M.r[rs] = 0xffffffffffffffffULL;
		if ((c / 12) % 4 == 2) M.r[rs] = 0x7fffffffffffffffULL;
		if (c & 16) M.r[rt] = M.r[rs];
		M.r[0] = 0;
		uint64_t regs[32];
		memcpy(regs, M.r, sizeof(regs));
		Mips3Insn I = {};
		I.pc = (c & 32) ? 0xff8 : (Random() & 0xffc);
		if (I.pc == 0xffc) I.pc = 0xff8;
		if (k < 2) I.op = Imm(opc[k], rs, rt, int16_t(Random()));
		else if (k < 4) I.op = Imm(opc[k], rs, 0, int16_t(Random()));
		else if (k < 8) I.op = Imm(1, rs, (k >= 6 ? 16 : 0) + (k & 1), int16_t(Random()));
		else if (k < 10) I.op = (opc[k] << 26) | (Random() & 0x03ffffff);
		else if (k < 12) I.op = R(rs, 0, k == 11 ? (c & 64 ? rs : rd) : 0, 0, k == 11 ? 9 : 8);
		else if (k < 14) I.op = Imm(opc[k], rs, rt, int16_t(Random()));
		else if (k < 16) I.op = Imm(opc[k], rs, 0, int16_t(Random()));
		else I.op = Imm(1, rs, (k >= 18 ? 18 : 2) + (k & 1), int16_t(Random()));
		UINT32 flags = 0;
		I.fn = Mips3Decode(I.op, &flags);
		InsnFields(&I);
		Require(flags & F_BRANCH, "invalid synthetic branch");
		Mx.vpage = uint32_t(Random()) & ~0xfffU;
		Mx.next_pc = 1;
		Mx.brk = 0;
		I.fn(&I);
		const bool annul = Mx.brk != 0;
		const bool likely = NativeLikely(I.fn);
		uint32_t next = Mx.next_pc;
		Mips3Insn delay = {};
		delay.op = RandomOp(c % 35);
		delay.pc = I.pc + 4; delay.flags = F_DELAY;
		delay.fn = Mips3Decode(delay.op, &flags);
		InsnFields(&delay);
		if (!annul) delay.fn(&delay);
		uint32_t words[128], offset = 0;
		words[0] = Mips3Arm64Jit::Entry;
		words[1] = Mips3Arm64Jit::MoveW(15, 6);
		words[2] = Mips3Arm64Jit::MoveW(16, 5);
		Mips3Arm64Jit::SpanEmitter emitter(words + 3);
		emitter.Branch(I.op, I.pc);
		if (likely) emitter.Annul(I.pc, 1, 0, 1, 0);
		Require(emitter.Alu(delay.op), "delay slot emit failed");
		emitter.Finish(2, 0, true, 1, 0, true);
		Require(cache.Publish(words, emitter.End() - words, offset), "publish branch pair");
		const uint64_t result = cache.RunFlow(offset, regs, HostRead, HostWrite, Mx.vpage);
		Require(result == ((uint64_t(next) << 32) | (1u << 25) |
			(annul ? Mips3Arm64Jit::EarlyExit | 1u : 2u)), "native branch target differs");
		Require(!memcmp(regs, M.r, sizeof(regs)), "branch link or delay register differs");
	}
	printf("Native flow type %d: 4096 branch/delay comparisons OK\n", type);
}

// Exercise a native RAM delay hit/miss and the fused superblock event
// boundary in the same generated code. The latter must not run on a taken
// branch or when the original op_SbExit would have stopped for an event.
static void TestFlowGuardAndBudget()
{
	using namespace Mips3Arm64Jit;
	Cache cache;
	Mips3HostEntry read[MIPS3_HOST_ENTRIES] = {}, write[MIPS3_HOST_ENTRIES] = {};
	alignas(8) uint32_t ram[1024] = {};
	ram[0] = 0x87654321;
	const uint32_t page = 0x80001000;
	const unsigned idx = (page >> 12) & (MIPS3_HOST_ENTRIES - 1);
	read[idx].host = reinterpret_cast<UINT8 *>(ram);
	uint32_t words[256], off = 0;
	words[0] = Entry; words[1] = MoveW(15,6); words[2] = MoveW(16,5);
	SpanEmitter e(words + 3);
	Require(e.Alu(Imm(9,0,8,7)), "prefix");
	e.Branch(Imm(4,1,2,6), 4);
	Require(e.Memory(Imm(0x23,3,4,0),2,0,1,0,true), "native delay RAM");
	e.Boundary(12,3,4,1,1,1);
	Require(e.Alu(Imm(9,8,8,1)), "suffix");
	e.Branch(Imm(5,1,2,4),16);
	Require(e.Alu(Imm(9,8,8,2)), "second delay");
	e.Finish(7,1,true,2,1,true);
	Require(cache.Publish(words,e.End()-words,off),"flow guard publication");
	for(unsigned hit=0;hit<2;hit++) for(unsigned taken=0;taken<2;taken++) for(unsigned enough=0;enough<2;enough++) {
		uint64_t regs[32]={};
		regs[1]=1; regs[2]=taken?1:2; regs[3]=page;
		read[idx].tag=hit?page:~0u;
		uint64_t result=cache.RunFlow(off,regs,read,write,0x80000000,enough?100:3);
		uint32_t expectTarget=taken?0x80000020u:1u;
		uint32_t low;
		if (!hit) {
			low=2u | (1u<<25) | DelayGuard;
			Require(regs[8]==7 && regs[4]==0,"delay miss changed state");
		} else if (taken || !enough) {
			low=4u | (1u<<9) | (1u<<16) | (1u<<25) | EarlyExit;
			if (!taken) expectTarget=0x8000000cu;
			Require(regs[8]==7 && regs[4]==0xffffffff87654321ULL,"boundary state");
		} else {
			low=7u | (1u<<9) | (1u<<16) | (2u<<25);
			expectTarget=0x80000024u;
			Require(regs[8]==10 && regs[4]==0xffffffff87654321ULL,"fallthrough state");
		}
		Require(result==((uint64_t(expectTarget)<<32)|low),"flow delay guard/event metadata differs");
	}
	puts("Native flow RAM delay, two branches and event boundary: PASS");
}

static UINT32 *mappedData;
static unsigned mappedReads, mappedWrites, trackedWrites;
static UINT32 __fastcall TestReadWord(UINT32 a) { mappedReads++; return BURN_ENDIAN_SWAP_INT32(mappedData[(a & 0xfff) / 4]); }
static void __fastcall TestWriteWord(UINT32 a, UINT32 v) { mappedWrites++; mappedData[(a & 0xfff) / 4] = BURN_ENDIAN_SWAP_INT32(v); }
static void TestTrackWrite(UINT32) { trackedWrites++; }

struct Snapshot {
	Mips3State state;
	UINT32 data;
	INT32 cycleDebt;
	unsigned mappedReads, mappedWrites, trackedWrites;
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
		// Loop: guarded load/store and ALU; branch/delay remain interpreted.
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
	mappedData = ram + 0x1000 / 4;
	mappedReads = mappedWrites = trackedWrites = 0;
	UINT8 tracked[2] = {};
	Mips3SetWriteTrack(tracked, 0x2000, TestTrackWrite);
	Mips3SetReadWordHandler(3, TestReadWord);
	Mips3SetWriteWordHandler(3, TestWriteWord);
	for (unsigned stage = 0; stage < 5; stage++) {
		for (unsigned i = 0; i < 31; i++) {
			Mips3Run(17 + i % 13);
			Snapshot snap = {};
			snap.state = M;
			snap.data = ram[0x1000/4];
			snap.cycleDebt = nMips3ICount;
			snap.mappedReads = mappedReads;
			snap.mappedWrites = mappedWrites;
			snap.trackedWrites = trackedWrites;
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
		} else if (stage == 2) {
			// Y2 protects RAM this way while the renderer owns the page.
			Mips3BeginMapBatch();
			Mips3MapHandler(3, 0x1000, 0x1fff, MAP_READ);
			Mips3BeginMapBatch();
			Mips3MapHandler(3, 0x1000, 0x1fff, MAP_WRITE);
			Mips3EndMapBatch();
			Mips3EndMapBatch();
		} else if (stage == 3) {
			Require(mappedReads > 0 && mappedWrites > 0, "protected-page handler was bypassed");
			Mips3MapMemory((UINT8 *)mappedData, 0x1000, 0x1fff, MAP_RAM);
			tracked[1] = 0;
			Mips3SetWriteTrack(tracked, 0x2000, TestTrackWrite);
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
		if (bMips3FastRam) {
			Require(nJitRamHits > 0 && nJitRamMisses > 0, "guarded RAM hits/exits not exercised");
			printf("Core type %d: %llu native RAM hits, %llu side exits\n", type,
				(unsigned long long)nJitRamHits, (unsigned long long)nJitRamMisses);
		}
		if (bMips3Integrated) {
			Require(nJitBranches > 0, "native branches not exercised");
			printf("Core type %d: %llu native branches\n", type, (unsigned long long)nJitBranches);
		}
		Require(nJitSpansBuilt > 0 && nJitSpansRun > 0 && nJitInsns > 0, "no native span executed");
		printf("Core type %d: %llu native spans run, %llu guest instructions\n", type,
			(unsigned long long)nJitSpansRun, (unsigned long long)nJitInsns);
	}
	Require(trackedWrites >= 2, "first-write tracker was bypassed");
	Mips3SetWriteTrack(NULL, 0, NULL);
	Mips3Exit();
	free(ram);
	return result;
}

static Mips3State RunFlowCase(int type, bool enable, unsigned scenario)
{
	UINT32 *ram = (UINT32 *)calloc(0x2000, 1);
	Require(ram != NULL, "flow case allocation");
	// The first branch is an ordinary forward condition with a delay ALU.
	// The second branch's slot loads RAM, including misalignment/TLB faults.
	const uint32_t code[] = {
		Imm(9,0,1,1), Imm(9,0,2,scenario & 1),
		Imm(4,1,2,3), Imm(9,0,5,12),
		Imm(9,0,6,23), Imm(9,0,7,34),
		Imm(9,0,8,45),
		Imm(scenario & 2 ? 0x14 : 4,1,2,2),
		Imm(0x23,3,4,scenario & 4 ? 1 : 0),
		Imm(9,0,9,56), 0x0800000a, 0
	};
	for (unsigned i=0; i<sizeof(code)/sizeof(code[0]); i++) ram[i]=BURN_ENDIAN_SWAP_INT32(code[i]);
	ram[0x1000/4] = BURN_ENDIAN_SWAP_INT32(0xabcdef12u);
	Mips3Init(type);
	Require(Mips3UseRecompiler(enable)==0, "flow case backend");
	Mips3MapMemory((UINT8 *)ram,0,0x1fff,MAP_RAM);
	Mips3Reset();
	M.pc = 0x80000000;
	M.r[3] = scenario & 8 ? 0x12345000u : 0xffffffff80001000ULL;
	// Exercise boundary exactly at and beyond the first branch's delay.
	Mips3Run(scenario & 16 ? 4 : 64);
	if (scenario & 16) Mips3Run(64);
	Mips3State state = M;
	Mips3Exit();
	free(ram);
	return state;
}

static void TestFlowCases(int type)
{
	for (unsigned scenario=0; scenario<32; scenario++) {
		Mips3State a=RunFlowCase(type,false,scenario), b=RunFlowCase(type,true,scenario);
		Require(!memcmp(&a,&b,sizeof(a)), "superblock/likely/delay fault CPU state differs");
	}
	printf("Flow core type %d: 32 event/likely/delay exception cases identical\n",type);
}

int main()
{
	TestPackedCache();
	TestFlowGuardAndBudget();
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
		TestGuardedMemoryEmitter(type);
		TestIntegratedMemory(type);
		TestNativeBranches(type);
		TestFlowCases(type);
		std::vector<Snapshot> a = RunProgram(type, false), b = RunProgram(type, true);
		Require(a.size() == b.size(), "snapshot count differs");
		for (size_t i = 0; i < a.size(); i++) {
			Require(!memcmp(&a[i].state, &b[i].state, sizeof(Mips3State)), "CPU state differs");
			Require(a[i].data == b[i].data && a[i].cycleDebt == b[i].cycleDebt, "memory or cycle debt differs");
			Require(a[i].mappedReads == b[i].mappedReads && a[i].mappedWrites == b[i].mappedWrites &&
				a[i].trackedWrites == b[i].trackedWrites, "device or write tracking differs");
		}
		printf("Core type %d: %zu interpreter/JIT snapshots identical\n", type, a.size());
	}
	puts("PASS: ARM64 register cache + guarded RAM + native branches + mixed-block differential tests");
	return 0;
}
