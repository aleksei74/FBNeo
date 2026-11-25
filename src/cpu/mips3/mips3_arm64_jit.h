// Experimental ARM64 backend for the shared MIPS3/VR5500 cached interpreter.
// SPDX-License-Identifier: BSD-3-Clause
// Only non-trapping GPR ALU operations are emitted here. The existing core
// owns PC, cycles, branches, exceptions, TLB, memory and device side effects.
#ifndef FBNEO_MIPS3_ARM64_JIT_H
#define FBNEO_MIPS3_ARM64_JIT_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

namespace Mips3Arm64Jit {

// AAPCS64: x0 = GPR array, x1/x2 = scratch. No callee-saved registers,
// stack changes, indirect guest accesses, or calls into the interpreter.
static const uint32_t Entry = 0xd503245f; // BTI c (hint on older CPUs)
static const uint32_t Ret = 0xd65f03c0;
static const unsigned MaxWordsPerInsn = 6;

inline uint32_t Load(unsigned host, unsigned guest)
{
	return guest ? (0xf9400000u | (guest << 10) | host) : (0xd2800000u | host);
}

inline uint32_t Store(unsigned guest)
{
	return 0xf9000001u | (guest << 10); // STR x1, [x0, #guest*8]
}

inline uint32_t Immediate(unsigned host, uint16_t imm, bool sign)
{
	if (sign && (imm & 0x8000)) return 0x92800000u | (uint32_t(uint16_t(~imm)) << 5) | host;
	return 0xd2800000u | (uint32_t(imm) << 5) | host;
}

// Emit at most six words. -1 means unsupported; zero means a no-op.
// Reserved encodings and VR5500 rotate forms are left to the interpreter.
inline int Emit(uint32_t op, uint32_t *out)
{
	const unsigned opc = op >> 26, rs = (op >> 21) & 31, rt = (op >> 16) & 31;
	const unsigned rd = (op >> 11) & 31, sa = (op >> 6) & 31, fn = op & 63;
	const uint16_t imm = uint16_t(op);
	uint32_t alu = 0;
	unsigned dst = rt;
	bool narrow = false, compare = false, unsignedCompare = false;
	int n = 0;
	if (opc == 0) {
		dst = rd;
		switch (fn) {
			case 0x00: case 0x02: case 0x03:
			case 0x38: case 0x3a: case 0x3b:
			case 0x3c: case 0x3e: case 0x3f: {
				if (rs) return -1;
				if (!dst) return 0;
				narrow = fn <= 3;
				unsigned shift = sa + (fn >= 0x3c ? 32 : 0);
				alu = (narrow ? 0x1ac02000u : 0x9ac02000u);
				if ((fn & 3) == 2) alu += 0x400;
				if ((fn & 3) == 3) alu += 0x800;
				out[n++] = Load(1, rt);
				out[n++] = Immediate(2, uint16_t(shift), false);
				break;
			}
			case 0x04: case 0x06: case 0x07:
			case 0x14: case 0x16: case 0x17:
				if (sa) return -1;
				if (!dst) return 0;
				narrow = fn < 0x10;
				alu = narrow ? 0x1ac02000u : 0x9ac02000u;
				if ((fn & 3) == 2) alu += 0x400;
				if ((fn & 3) == 3) alu += 0x800;
				out[n++] = Load(1, rt);
				out[n++] = Load(2, rs);
				break;
			default:
				if (sa) return -1;
				switch (fn) {
					case 0x0a: case 0x0b: // MOVZ / MOVN: preserve old destination
						if (!dst) return 0;
						out[n++] = Load(1, rs);
						out[n++] = Load(2, rt);
						out[n++] = 0xf100005fu; // CMP x2,#0
						out[n++] = Load(2, dst);
						out[n++] = 0x9a820021u | (fn == 0x0b ? 0x1000u : 0); // CSEL x1,x1,x2,EQ/NE
						out[n++] = Store(dst);
						return n;
					case 0x21: alu = 0x0b000000u; narrow = true; break; // ADDU
					case 0x23: alu = 0x4b000000u; narrow = true; break; // SUBU
					case 0x24: alu = 0x8a000000u; break; // AND
					case 0x25: alu = 0xaa000000u; break; // OR
					case 0x26: alu = 0xca000000u; break; // XOR
					case 0x27: alu = 0xaa000000u; break; // NOR = OR + MVN
					case 0x2a: compare = true; break;
					case 0x2b: compare = unsignedCompare = true; break;
					case 0x2d: alu = 0x8b000000u; break; // DADDU
					case 0x2f: alu = 0xcb000000u; break; // DSUBU
					default: return -1;
				}
				if (!dst) return 0;
				out[n++] = Load(1, rs);
				out[n++] = Load(2, rt);
				break;
		}
	} else if (opc == 0x0f) {
		if (rs) return -1;
		if (!dst) return 0;
		out[n++] = 0x52a00001u | (uint32_t(imm) << 5); // MOVZ w1,#imm,LSL #16
		out[n++] = 0x93407c21u; // SXTW x1,w1
		out[n++] = Store(dst);
		return n;
	} else {
		switch (opc) {
			case 0x09: alu = 0x0b000000u; narrow = true; break;
			case 0x0a: compare = true; break;
			case 0x0b: compare = unsignedCompare = true; break;
			case 0x0c: alu = 0x8a000000u; break;
			case 0x0d: alu = 0xaa000000u; break;
			case 0x0e: alu = 0xca000000u; break;
			case 0x19: alu = 0x8b000000u; break;
			default: return -1;
		}
		if (!dst) return 0;
		out[n++] = Load(1, rs);
		out[n++] = Immediate(2, imm, opc == 0x09 || opc == 0x19 || compare);
	}
	if (compare) {
		out[n++] = 0xeb02003fu; // CMP x1,x2
		out[n++] = unsignedCompare ? 0x1a9f27e1u : 0x1a9fa7e1u; // CSET w1,LO/LT
	} else {
		out[n++] = alu | (2u << 16) | (1u << 5) | 1u;
		if (opc == 0 && fn == 0x27) out[n++] = 0xaa2103e1u; // MVN x1,x1
		if (narrow) out[n++] = 0x93407c21u;
	}
	out[n++] = Store(dst);
	return n;
}

// Adjacent native instructions can exchange a result in x1 instead of
// reloading it from the guest register file. A following instruction that
// overwrites the previous destination also makes its store unobservable.
// Only rewrite complete instruction sequences and never cross a span boundary.
inline int EmitAdjacent(uint32_t op, uint32_t *out, bool hasPrevious)
{
	int n = Emit(op, out);
	if (n <= 0 || !hasPrevious) return n;

	const uint32_t prev = out[-1];
	if ((prev & ~uint32_t(31u << 10)) != Store(0)) return n;
	const unsigned previousGuest = (prev >> 10) & 31;
	const bool forwardsX1 = out[0] == Load(1, previousGuest);

	// x1 already holds the value from the preceding store. Every other guest
	// read must still come from the register array, including MOVZ/MOVN
	// condition reads and second operands loaded into x2.
	bool otherRead = false;
	for (int i = forwardsX1 ? 1 : 0; i < n; i++) {
		if (out[i] == Load(2, previousGuest)) {
			otherRead = true;
			break;
		}
	}

	if (forwardsX1 && !otherRead && out[n - 1] == Store(previousGuest)) {
		// Remove both the dead store and the forwarded reload.
		memmove(out - 1, out + 1, size_t(n - 1) * sizeof(uint32_t));
		return n - 2;
	}
	if (forwardsX1) {
		memmove(out, out + 1, size_t(--n) * sizeof(uint32_t));
		return n;
	}
	if (!otherRead && out[n - 1] == Store(previousGuest)) {
		memmove(out - 1, out, size_t(n) * sizeof(uint32_t));
		return n - 1;
	}
	return n;
}

// Bounded W^X code cache: publish whole page-aligned batches, never rewrite
// executable pages. Exhaustion or a denied executable mapping means fallback.
// Lifetime follows the interpreter's decoded-block arena, not guest state.
class Cache {
	uint8_t *base_;
	size_t used_, page_;
	bool denied_;
	static const size_t Capacity = 32u * 1024u * 1024u;
	Cache(const Cache&);
	Cache& operator=(const Cache&);
public:
	Cache() : base_(NULL), used_(0), page_(0), denied_(false) {}
	~Cache() { Reset(); }
	void Reset() {
		if (base_) munmap(base_, Capacity);
		base_ = NULL; used_ = 0; page_ = 0;
		// A permission failure is sticky until the next explicit enable/init.
	}
	void Enable() { denied_ = false; }
	bool Publish(const uint32_t *words, size_t count, uint32_t &offset) {
		if (!count || denied_) return false;
		if (!base_) {
			long page = sysconf(_SC_PAGESIZE);
			if (page <= 0 || Capacity % size_t(page)) { denied_ = true; return false; }
			page_ = size_t(page);
			void *p = mmap(NULL, Capacity, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
			if (p == MAP_FAILED) { denied_ = true; return false; }
			base_ = static_cast<uint8_t *>(p);
		}
		if (count > Capacity / sizeof(uint32_t)) return false;
		size_t bytes = count * sizeof(uint32_t);
		size_t rounded = ((bytes + page_ - 1) / page_) * page_;
		if (rounded > Capacity - used_) return false;
		uint8_t *dst = base_ + used_;
		memcpy(dst, words, bytes);
		__builtin___clear_cache(reinterpret_cast<char *>(dst), reinterpret_cast<char *>(dst + bytes));
		if (mprotect(dst, rounded, PROT_READ | PROT_EXEC)) { denied_ = true; return false; }
		offset = uint32_t(used_);
		used_ += rounded;
		return true;
	}
	void Run(uint32_t offset, uint64_t *r) const {
		reinterpret_cast<void (*)(uint64_t *)>(base_ + offset)(r);
	}
	bool Denied() const { return denied_; }
	size_t Used() const { return used_; }
};

} // namespace Mips3Arm64Jit
#endif
