// SPDX-License-Identifier: BSD-3-Clause
// Independent extension of FBNeo's ARM64 backend, inspired by MAME's
// register mapping and delay-slot compilation. A span owns only caller-saved
// x9..x14 and commits architectural GPRs at each side exit and normal return.
#ifndef FBNEO_MIPS3_ARM64_INTEGRATED_H
#define FBNEO_MIPS3_ARM64_INTEGRATED_H

namespace Mips3Arm64Jit {

static const unsigned FlowFlag = 0x8000;
static const unsigned TerminalFlag = 0x4000;
static const unsigned FpuFlag = 0x2000;
// Flow result low 32 bits: 9 decoded entries, 6 pseudo-ops, early-exit
// flag, 9 RAM hits, 6 completed branches, delay-guard flag. Upper 32
// bits contain the runtime next PC when applicable.
static const uint32_t EarlyExit = 0x00008000u;
static const uint32_t DelayGuard = 0x80000000u;
static const unsigned IntegratedMaxWords = 96;

inline uint32_t MoveX(unsigned d, unsigned s) { return 0xaa0003e0u | (s << 16) | d; }
inline uint32_t MoveW(unsigned d, unsigned s) { return 0x2a0003e0u | (s << 16) | d; }
inline uint32_t StoreReg(unsigned guest, unsigned host) { return 0xf9000000u | (guest << 10) | host; }
inline uint32_t Sxtw(unsigned d) { return 0x93407c00u | (d << 5) | d; }
inline uint32_t CmpX(unsigned a, unsigned b) { return 0xeb00001fu | (b << 16) | (a << 5); }
inline uint32_t Mask(unsigned r) { return 1u << r; }

class SpanEmitter {
	struct Slot { unsigned guest, age; bool dirty; } slot_[6];
	unsigned age_;
	uint32_t *p_;
	bool fpuChecked_;
	unsigned Acquire(unsigned guest, bool load, uint32_t pinned)
	{
		if (!guest) return 31; // ZR; never allocate guest r0
		unsigned choice = 6;
		for (unsigned i = 0; i < 6; i++) {
			if (slot_[i].guest == guest) {
				slot_[i].age = ++age_;
				if (!load) slot_[i].dirty = true;
				return i + 9;
			}
			if (!(pinned & Mask(i + 9)) &&
				(choice == 6 || !slot_[i].guest || (slot_[choice].guest && slot_[i].age < slot_[choice].age)))
				choice = i;
		}
		// At most three source registers are pinned by one guest instruction.
		if (slot_[choice].dirty) *p_++ = StoreReg(slot_[choice].guest, choice + 9);
		slot_[choice] = {guest, ++age_, !load};
		if (load) *p_++ = Load(choice + 9, guest);
		return choice + 9;
	}
	unsigned Read(unsigned g, uint32_t pinned = 0) { return Acquire(g, true, pinned); }
	unsigned Write(unsigned g, uint32_t pinned = 0) { return Acquire(g, false, pinned); }
	void FlushTo(uint32_t *&p) const
	{
		for (unsigned i = 0; i < 6; i++)
			if (slot_[i].dirty) *p++ = StoreReg(slot_[i].guest, i + 9);
	}
	void Imm32(unsigned d, uint32_t v)
	{
		*p_++ = 0x52800000u | ((v & 0xffffu) << 5) | d;
		if (v >> 16) *p_++ = 0x72a00000u | ((v >> 16) << 5) | d;
	}
	void PcPlus(unsigned d, uint32_t off)
	{
		Imm32(d, off);
		*p_++ = 0x0b000000u | (d << 16) | (16u << 5) | d; // ADD wd,w16,wd
	}
	void Return(unsigned completed, unsigned memory, unsigned branches, unsigned pseudo,
		bool target, bool early = false, bool delay = false)
	{
		p_ += EmitReturn(p_, completed | (pseudo << 9) | (early ? 0x8000u : 0),
			memory | (branches << 9) | (delay ? 0x8000u : 0));
		if (target) {
			p_[-1] = 0xaa118000u; // ORR x0,x0,x17,LSL #32
			*p_++ = Ret;
		}
	}
	static void PatchCond(uint32_t *at, const uint32_t *target, unsigned cond)
	{
		*at = 0x54000000u | (uint32_t(target - at) << 5) | cond;
	}
public:
	explicit SpanEmitter(uint32_t *p) : slot_{}, age_(0), p_(p), fpuChecked_(false) {}
	uint32_t *End() const { return p_; }
	static bool SupportsAlu(uint32_t op) { uint32_t tmp[MaxWordsPerInsn]; return Emit(op, tmp) >= 0; }

	// Emit() remains the authority for supported/illegal encodings. Emit the
	// same operations directly on allocated host registers; no GPR stores
	// until eviction or a side exit, and no dirty state crosses a span.
	bool Alu(uint32_t op)
	{
		uint32_t tmp[MaxWordsPerInsn];
		const int n = Emit(op, tmp);
		if (n < 0) return false;
		if (!n) return true;
		const unsigned opc = op >> 26, rs = (op >> 21) & 31, rt = (op >> 16) & 31;
		const unsigned rd = (op >> 11) & 31, sa = (op >> 6) & 31, fn = op & 63;
		const unsigned dst = opc ? rt : rd;
		const uint16_t imm = uint16_t(op);
		unsigned a, b, d;
		uint32_t alu = 0;
		bool narrow = false, compare = false, ucmp = false, invert = false;

		if (opc == 0) {
			switch (fn) {
				case 0x00: case 0x02: case 0x03:
				case 0x38: case 0x3a: case 0x3b:
				case 0x3c: case 0x3e: case 0x3f:
				case 0x04: case 0x06: case 0x07:
				case 0x14: case 0x16: case 0x17: {
					const bool variable = fn == 4 || fn == 6 || fn == 7 || fn == 0x14 || fn == 0x16 || fn == 0x17;
					narrow = fn < 0x10;
					a = Read(rt);
					if (variable) b = Read(rs, Mask(a));
					else {
						b = 2;
						*p_++ = Immediate(b, uint16_t(sa + (fn >= 0x3c ? 32 : 0)), false);
					}
					alu = narrow ? 0x1ac02000u : 0x9ac02000u;
					if ((fn & 3) == 2) alu += 0x400;
					if ((fn & 3) == 3) alu += 0x800;
					break;
				}
				case 0x0a: case 0x0b: {
					a = Read(rs);
					b = Read(rt, Mask(a));
					d = Read(dst, Mask(a) | Mask(b)); // conditional write preserves old dst
					Write(dst);
					*p_++ = CmpX(b, 31);
					*p_++ = 0x9a800000u | (d << 16) | (fn == 0x0b ? 0x1000u : 0) | (a << 5) | d;
					return true;
				}
				default:
					a = Read(rs);
					b = Read(rt, Mask(a));
					switch (fn) {
						case 0x21: alu = 0x0b000000u; narrow = true; break;
						case 0x23: alu = 0x4b000000u; narrow = true; break;
						case 0x24: alu = 0x8a000000u; break;
						case 0x25: alu = 0xaa000000u; break;
						case 0x26: alu = 0xca000000u; break;
						case 0x27: alu = 0xaa000000u; invert = true; break;
						case 0x2a: compare = true; break;
						case 0x2b: compare = ucmp = true; break;
						case 0x2d: alu = 0x8b000000u; break;
						case 0x2f: alu = 0xcb000000u; break;
						default: return false; // kept in sync with Emit()
					}
					break;
			}
		} else if (opc == 0x0f) {
			d = Write(dst);
			*p_++ = 0x52a00000u | (uint32_t(imm) << 5) | d;
			*p_++ = Sxtw(d);
			return true;
		} else {
			a = Read(rs);
			const int32_t si = int16_t(imm);
			if ((opc == 0x09 || opc == 0x19) && !rs) {
				d = Write(dst);
				*p_++ = Immediate(d, imm, true);
				return true;
			}
			if ((opc == 0x09 || opc == 0x19) && si >= -4095 && si <= 4095) {
				d = Write(dst, Mask(a));
				*p_++ = (opc == 0x09 ? 0x11000000u : 0x91000000u) |
					(si < 0 ? 0x40000000u : 0) | (uint32_t(si < 0 ? -si : si) << 10) | (a << 5) | d;
				if (opc == 0x09) *p_++ = Sxtw(d);
				return true;
			}
			switch (opc) {
				case 0x09: alu = 0x0b000000u; narrow = true; break;
				case 0x19: alu = 0x8b000000u; break;
				case 0x0a: compare = true; break;
				case 0x0b: compare = ucmp = true; break;
				case 0x0c: alu = 0x8a000000u; break;
				case 0x0d: alu = 0xaa000000u; break;
				case 0x0e: alu = 0xca000000u; break;
				default: return false;
			}
			b = 2;
			*p_++ = Immediate(b, imm, opc == 0x09 || opc == 0x19 || compare);
		}
		d = Write(dst, Mask(a) | Mask(b));
		if (compare) {
			*p_++ = CmpX(a, b);
			*p_++ = (ucmp ? 0x1a9f27e0u : 0x1a9fa7e0u) | d;
		} else {
			*p_++ = alu | (b << 16) | (a << 5) | d;
			if (invert) *p_++ = 0xaa2003e0u | (d << 16) | d;
			if (narrow) *p_++ = Sxtw(d);
		}
		return true;
	}

	bool Memory(uint32_t op, unsigned completed, unsigned memory, unsigned branches = 0, unsigned pseudo = 0, bool delay = false)
	{
		// Reuse the exact existing address/tag/alignment guard and access
		// encodings. Only replace GPR traffic and extend the failure exit.
		uint32_t tmp[MaxMemoryWords];
		const int n = EmitMemory(op, tmp, completed, memory);
		if (n < 0) return false;
		const unsigned rs = (op >> 21) & 31, rt = (op >> 16) & 31, opc = op >> 26;
		const bool write = opc == 0x28 || opc == 0x29 || opc == 0x2b || opc == 0x3f;
		int branch = 1;
		while (branch < n && tmp[branch] != 0x54000080u) branch++;
		if (branch == n) return false;
		const unsigned base = Read(rs);
		*p_++ = MoveW(5, base);
		for (int i = 1; i < branch; i++) *p_++ = tmp[i];
		uint32_t *guard = p_++;
		FlushTo(p_); // precise GPR state if the handler observes an exception/MMIO
		Return(completed, memory, branches, pseudo, delay, false, delay);
		PatchCond(guard, p_, 0); // B.EQ
		*p_++ = tmp[branch + 4]; // host pointer
		*p_++ = tmp[branch + 5]; // within-page offset
		if (write) {
			unsigned r = Read(rt);
			*p_++ = (tmp[n - 1] & ~31u) | r;
		} else {
			unsigned r = Write(rt);
			*p_++ = (tmp[n - 2] & ~31u) | r;
		}
		return true;
	}

	// Guarded COP1 operations specialized for the current FR mode.
	// Never modify FPR/GPR state before all guards pass. A changed
	// register layout, disabled CU1, or exceptional FP values use the
	// exact original interpreter handlers.
	bool Fpu(uint32_t op,unsigned srOff,unsigned fprOff,unsigned completed,unsigned memory,
		unsigned branches=0,unsigned pseudo=0,bool fr=true)
	{
		if((op>>26)!=0x11)return false;
		const unsigned fmt=(op>>21)&31,rt=(op>>16)&31,fs=(op>>11)&31,fd=(op>>6)&31,fn=op&63;
		const bool transfer=fmt==0 || fmt==1 || fmt==4 || fmt==5;
		const bool dbl=fmt==1 || fmt==5 || fmt==17;
		const bool arith=(fmt==16 || fmt==17) && fn<=4;
		const bool sign=(fmt==16 || fmt==17) && fn>=5 && fn<=7;
		if(!transfer && !arith && !sign)return false;
		const auto offset=[&](unsigned reg,bool wide)->unsigned{
			return fprOff+(fr?reg*8:(reg&~1u)*8+(!wide?(reg&1)*4:0));
		};
		const auto ld=[&](unsigned r,unsigned reg,bool wide){
			*p_++=(wide?0xf9400000u:0xb9400000u)|((offset(reg,wide)>>(wide?3:2))<<10)|r;
		};
		const auto st=[&](unsigned r,unsigned reg,bool wide){
			// FR=1 single writes clear the upper word; FR=0 singles
			// write exactly one half of the corresponding even pair.
			const bool full=wide||fr;
			*p_++=(full?0xf9000000u:0xb9000000u)|((offset(reg,wide)>>(full?3:2))<<10)|r;
		};
		uint32_t *guards[4];unsigned n=0;
		unsigned conditions[4];
		if(!fpuChecked_){
			*p_++=0xb9400000u|((srOff>>2)<<10)|2u; // LDR w2,Status
			Imm32(6,0x24000000u); // CU1 and FR
			*p_++=0x0a060042u; // AND w2,w2,w6
			if(!fr)Imm32(6,0x20000000u);
			*p_++=0x6b06005fu; // CMP w2,w6
			guards[n]=p_++;conditions[n++]=1; // B.NE
			fpuChecked_=true;
		}
		auto normal=[&](unsigned r){
			*p_++=(dbl?0xd3400000u:0x53000000u)|((dbl?52u:23u)<<16)|((dbl?62u:30u)<<10)|(r<<5)|5u;
			*p_++=0x510004a5u; // SUB w5,w5,#1
			*p_++=0x7100001fu|((dbl?2045u:253u)<<10)|(5u<<5);
			guards[n]=p_++;conditions[n++]=8; // B.HI
		};
		if(arith){
			ld(2,fs,dbl);normal(2);
			*p_++=(dbl?0x9e670000u:0x1e270000u)|(2u<<5); // FMOV d0/x2 or s0/w2
			if(fn==4){
				*p_++=(dbl?0x1e61c000u:0x1e21c000u); // FSQRT d0,d0 / s0,s0
			}else{
				ld(6,rt,dbl);normal(6);
				*p_++=(dbl?0x9e670000u:0x1e270000u)|(6u<<5)|1u;
				static const uint32_t ops[4]={0x1e202800u,0x1e203800u,0x1e200800u,0x1e201800u};
				*p_++=ops[fn]|(dbl?0x00400000u:0)|(1u<<16);
			}
			*p_++=(dbl?0x9e660000u:0x1e260000u)|2u;
			normal(2);
		} else if(sign){
			ld(2,fs,dbl);
			if(fn!=6){
				if(dbl)*p_++=0xd2e00000u|(0x8000u<<5)|6u;
				else Imm32(6,0x80000000u);
				*p_++=(fn==5?0x8a200000u:0xca000000u)|(6u<<16)|(2u<<5)|2u;
			}
		}
		if(n){
			uint32_t *skip=p_++;
			for(unsigned i=0;i<n;i++)PatchCond(guards[i],p_,conditions[i]);
			FlushTo(p_);
			Return(completed,memory,branches,pseudo,false);
			*skip=0x14000000u|uint32_t(p_-skip);
		}
		if(transfer){
			if(fmt<=1){
				if(rt){
					const unsigned d=Write(rt);
					ld(d,fs,dbl);
					if(!dbl)*p_++=Sxtw(d);
				}
			}else{
				const unsigned r=Read(rt);
				if(!dbl){*p_++=MoveW(2,r);st(2,fs,false);}
				else st(r,fs,true);
			}
		}else st(2,fd,dbl);
		return true;
	}

	// A normal branch + non-trapping ALU delay slot executes in one span.
	// w15 is the event budget, w16 the runtime page, w17 the decision across the
	// delay slot; the caller still performs the original event/block checks.
	void Branch(uint32_t op, unsigned pc)
	{
		const unsigned raw = op >> 26, rs = (op >> 21) & 31, rt = (op >> 16) & 31, rd = (op >> 11) & 31;
		const unsigned opc = raw >= 0x14 && raw <= 0x17 ? raw - 0x10 : raw;
		unsigned link = 0;
		if (opc == 0) {
			unsigned a = Read(rs);
			*p_++ = MoveW(17, a);
			if ((op & 63) == 9) link = rd;
		} else if (opc == 2 || opc == 3) {
			PcPlus(17, pc + 4);
			Imm32(2, 0xf0000000u);
			*p_++ = 0x0a000000u | (2u << 16) | (17u << 5) | 17u;
			Imm32(2, (op & 0x03ffffffu) << 2);
			*p_++ = 0x2a000000u | (2u << 16) | (17u << 5) | 17u;
			if (opc == 3) link = 31;
		} else {
			unsigned a = Read(rs), b = 31, cond = 0;
			if (opc == 4 || opc == 5) { b = Read(rt, Mask(a)); cond = opc == 4 ? 0 : 1; }
			else if (opc == 6 || opc == 7) cond = opc == 6 ? 13 : 12; // LE/GT
			else {
				cond = (rt & 1) ? 10 : 11; // GE/LT
				if (rt >= 16 && rt <= 19) link = 31;
			}
			*p_++ = CmpX(a, b);
			PcPlus(17, uint32_t(int32_t(int16_t(op)) * 4 + int32_t(pc) + 4));
			*p_++ = Immediate(2, 1, false);
			*p_++ = 0x1a800000u | (2u << 16) | (cond << 12) | (17u << 5) | 17u; // CSEL w17,w17,w2
		}
		if (link) {
			unsigned d = Write(link);
			PcPlus(d, pc + 8);
			*p_++ = Sxtw(d);
		}
	}

	// Likely branches annul the delay slot when not taken. Link writes
	// (including the untaken REGIMM link case) are already in the cache.
	void Annul(unsigned pc, unsigned completed, unsigned memory, unsigned branches, unsigned pseudo)
	{
		*p_++ = 0x7100001fu | (1u << 10) | (17u << 5); // CMP w17,#1
		uint32_t *keep = p_++;
		PcPlus(17, pc + 8);
		FlushTo(p_);
		Return(completed, memory, branches, pseudo, true, true);
		PatchCond(keep, p_, 1); // B.NE: taken, execute delay
	}

	// Fuse the existing superblock exit into generated code. w15 holds
	// the available cycles at entry; pseudo-op cyc is cumulative in
	// the decoded block. A taken branch or expired deadline returns to C++.
	void Boundary(unsigned pc, unsigned cyc, unsigned completed, unsigned memory, unsigned branches, unsigned pseudo)
	{
		*p_++ = 0x7100001fu | (1u << 10) | (17u << 5); // CMP w17,#1
		uint32_t *taken = p_++;
		*p_++ = 0x7100001fu | (cyc << 10) | (15u << 5); // CMP w15,#cyc
		uint32_t *keep = p_++;
		PcPlus(17, pc);
		PatchCond(taken, p_, 1);
		FlushTo(p_);
		Return(completed, memory, branches, pseudo, true, true);
		PatchCond(keep, p_, 8); // B.HI: not taken, event budget still available
	}

	void Finish(unsigned completed, unsigned memory, bool flow, unsigned branches = 0, unsigned pseudo = 0, bool terminal = false, bool guarded = false)
	{
		FlushTo(p_);
		if (flow || memory || guarded) Return(completed, memory, branches, pseudo, terminal);
		else *p_++ = Ret;
	}
};

} // namespace Mips3Arm64Jit
#endif
