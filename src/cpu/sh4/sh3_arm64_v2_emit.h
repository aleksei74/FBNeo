// V2 stage B-2: compact blocks, live literals, DT and terminal branches.
// Guest arithmetic encodings follow the existing audited SH3 emitter.
// No generated code calls C, executes devices, or links to another block.
// Leaf blocks use the normal C ABI; wide blocks use a shared trampoline.
#ifndef FBNEO_SH3_ARM64_V2_EMIT_H
#define FBNEO_SH3_ARM64_V2_EMIT_H
#if SH3_NATIVE_V2
extern "C" unsigned fbneo_sh3_v2_enter(const void *,uint32_t *,uint32_t *,
	uint32_t *,const Sh3Native::Memory *);

// x19=GPR base, x20=SR address, x21=EA address, x22=memory context.
// Guest bindings use w9..w15 and w23..w28. Save all ABI-preserved registers
// once per invocation; no per-memory-op or per-guard prologue.
asm(
".pushsection .text.fbneo_sh3_v2_enter,\"ax\",%progbits\n"
".p2align 4\n"
".global fbneo_sh3_v2_enter\n"
".hidden fbneo_sh3_v2_enter\n"
".type fbneo_sh3_v2_enter,%function\n"
"fbneo_sh3_v2_enter:\n"
".cfi_startproc\n"
".inst 0xd503245f\n" // BTI c
"stp x19,x20,[sp,#-96]!\n"
".cfi_def_cfa_offset 96\n"
".cfi_offset x19,-96\n"
".cfi_offset x20,-88\n"
"stp x21,x22,[sp,#16]\n"
".cfi_offset x21,-80\n"
".cfi_offset x22,-72\n"
"stp x23,x24,[sp,#32]\n"
".cfi_offset x23,-64\n"
".cfi_offset x24,-56\n"
"stp x25,x26,[sp,#48]\n"
".cfi_offset x25,-48\n"
".cfi_offset x26,-40\n"
"stp x27,x28,[sp,#64]\n"
".cfi_offset x27,-32\n"
".cfi_offset x28,-24\n"
"str x30,[sp,#80]\n"
".cfi_offset x30,-16\n"
"str x29,[sp,#88]\n"
".cfi_offset x29,-8\n"
"mov x16,x0\n"
"mov x19,x1\n"
"mov x20,x2\n"
"mov x21,x3\n"
"mov x22,x4\n"
"blr x16\n"
"ldr x29,[sp,#88]\n"
".cfi_restore x29\n"
"ldr x30,[sp,#80]\n"
".cfi_restore x30\n"
"ldp x27,x28,[sp,#64]\n"
".cfi_restore x27\n"
".cfi_restore x28\n"
"ldp x25,x26,[sp,#48]\n"
".cfi_restore x25\n"
".cfi_restore x26\n"
"ldp x23,x24,[sp,#32]\n"
".cfi_restore x23\n"
".cfi_restore x24\n"
"ldp x21,x22,[sp,#16]\n"
".cfi_restore x21\n"
".cfi_restore x22\n"
"ldp x19,x20,[sp],#96\n"
".cfi_restore x19\n"
".cfi_restore x20\n"
".cfi_def_cfa_offset 0\n"
"ret\n"
".cfi_endproc\n"
".size fbneo_sh3_v2_enter,.-fbneo_sh3_v2_enter\n"
".popsection\n");

namespace Sh3V2 {
static constexpr unsigned CompactMaxOps=64,CompactMaxWords=64*CompactMaxOps+96;
inline bool Candidate(uint16_t op) {
	const unsigned top=op>>12, f=op&15, imm=op&255;
	if(top==1)return true;
	if(top==2)return f<=2||(f>=4 && f<=6)||(f>=8 && f<=11)||f==13;
	if(top==3)return f==0||f==2||f==3||f==6||f==7||f==8||f==12;
	if(top==4)return imm==0||imm==1||imm==8||imm==9||imm==0x11||imm==0x15||imm==0x18||imm==0x19||imm==0x20||imm==0x21||imm==0x28||imm==0x29;
	if(top==5||top==7||top==14)return true;
	if(top==6)return f!=10;
	return op==0x0009||op==0x0008||op==0x0018||(op&0xf0ff)==0x0029||
		(op&0xff00)==0x8800||(op&0xff00)==0xc800||(op&0xff00)==0xc900||(op&0xff00)==0xca00;
}
inline bool MemoryOp(uint16_t op) {
	unsigned t=op>>12, f=op&15;
	return t==1 || t==5 || ((t==2 || t==6) && (f<=2 || (f>=4 && f<=6)));
}

// Keep Candidate/MemoryOp unchanged for the B-1 and legacy control.
inline bool PcRelative(uint16_t op){return (op>>12)==9 || (op>>12)==13 || (op>>8)==0xc7;}
inline bool IsDT(uint16_t op){return (op&0xf0ff)==0x4010;}
inline bool ExtendedCandidate(uint16_t op){return Candidate(op)||PcRelative(op)||IsDT(op);}
inline bool Conditional(uint16_t op){
	const unsigned hi=op>>8;return hi==0x89||hi==0x8b||hi==0x8d||hi==0x8f;
}
inline bool Dynamic(uint16_t op){
	return (op&0xf0ff)==0x402b || (op&0xf0ff)==0x400b || op==0x000b;
}
inline bool Terminal(uint16_t op){
	return Conditional(op)||(op>>12)==10||(op>>12)==11||Dynamic(op);
}
inline bool Delayed(uint16_t op){return !Conditional(op)||bool(op&0x0400);}
inline bool SafeSlot(uint16_t op){
	return Candidate(op)&&!MemoryOp(op)&&(op&0xf0ff)!=0x4011;
}
inline uint32_t StaticTarget(uint16_t op,uint32_t pc){
	const int disp=Conditional(op)?int(int8_t(op)):int(op&0xfff)-((op&0x800)?4096:0);
	return pc+4+disp*2;
}
inline bool Call(uint16_t op){return (op>>12)==11||(op&0xf0ff)==0x400b;}
inline unsigned ExtraCycles(uint16_t op){
	return !Delayed(op)?2:(op&0xf0ff)==0x402b?0:1;
}
static constexpr unsigned ExitTaken=0x100,ExitNotTaken=0x200;
// C4-only additions. Keep C2/C3 decoding and self-repeat controls unchanged.
inline bool Tier2Memory(uint16_t op){
	const unsigned hi=op>>8, top=op>>12, f=op&15, key=op&0xf0ff;
	return hi==0x80||hi==0x81||hi==0x84||hi==0x85||
		(top==0 && ((f>=4&&f<=6)||(f>=12&&f<=14)))||
		(hi>=0xc0&&hi<=0xc2)||(hi>=0xc4&&hi<=0xc6)||
		key==0x4022||key==0x4026;
}
inline bool Tier2Candidate(uint16_t op){
	const unsigned key=op&0xf0ff, k=op&0xf00f;
	return Tier2Memory(op)||key==0x002a||key==0x402a||
		key==0x001a||key==0x401a||k==0x400c||k==0x400d||
		k==0x0007||k==0x200e||k==0x200f;
}
inline bool GlobalCandidate(uint16_t op){return ExtendedCandidate(op)||Tier2Candidate(op);}
inline unsigned GlobalExtra(uint16_t op){return (op&0xf00f)==0x0007?1:0;}
inline bool GlobalSafeSlot(uint16_t op){
	// The memory slot is preflighted before committing any branch effect.
	// PR access slots remain on the authoritative interpreter (call order).
	return SafeSlot(op)||MemoryOp(op)||
		(Tier2Memory(op)&&(op&0xf0ff)!=0x4022&&(op&0xf0ff)!=0x4026);
}
inline bool SlotWritesEA(uint16_t op){
	return Tier2Memory(op)||!((op>>12)==2&&(op&15)>=4)&&
		!((op>>12)==6&&(op&15)>=4);
}
struct GlobalStamp {uint64_t token,generation;const uint64_t *liveGeneration;};
struct GlobalBuild {
	const GlobalStamp *stamp;
	uint32_t *patch[3];uint32_t target[3];unsigned count;
	bool fastDT[64],trusted;
};
static_assert(sizeof(GlobalStamp)==24,"C4 stamp ABI");

// Fixed bindings allow a single union-of-writes epilogue, even when a
// guard exits before an instruction that would otherwise overwrite a
// register: every used register is initialized at entry.
struct ResidentFixup {
	uint32_t *at;
	uint32_t target;
	bool adr;
};
class CompactEmitter {
	uint32_t *p_;
	uint8_t reg_[16];
	uint16_t used_,dirty_;
	bool sr_,writeSR_,extended_,audit_,repeat_,resident_;
	uint32_t pc_;
	GlobalBuild *global_;
	unsigned maps_;
	uint8_t mapReg_[3],codeReg_,sizeReg_;
	unsigned RegBase()const{return Wide()?19:0;}
	unsigned SRBase()const{return Wide()?20:1;}
	unsigned EABase()const{return Wide()?21:2;}
	unsigned MemBase()const{return Wide()?22:3;}
	void LdrX(unsigned d,unsigned base,unsigned off){
		*p_++=0xf9400000u|((off/8)<<10)|(base<<5)|d;
	}
	struct Fixup {uint32_t *at;uint8_t cond,done;};
	Fixup fix_[CompactMaxOps*7];
	struct IdleFixup {uint32_t *at,*resume;uint8_t done;};
	IdleFixup idle_[CompactMaxOps];
	struct WatchFixup {uint32_t *at,*resume;};
	WatchFixup watch_[CompactMaxOps];
	unsigned fixCount_,idleCount_,watchCount_;
	uint32_t *entrySR_;
	bool failed_;
	static uint32_t Mov(unsigned d,unsigned s){return 0x2a0003e0u|(s<<16)|d;}
	unsigned R(unsigned g){return reg_[g];}
	unsigned W(unsigned g){dirty_|=1u<<g;return reg_[g];}
	void Imm(unsigned d,int v){
		*p_++=(v<0?0x12800000u:0x52800000u)|(uint32_t(v<0?~v:v)<<5)|d;
	}
	void Imm32(unsigned d,uint32_t v){
		*p_++=0x52800000u|((v&65535u)<<5)|d;
		if(v>>16)*p_++=0x72a00000u|((v>>16)<<5)|d;
	}
	void LdrW(unsigned d,unsigned base,unsigned off){
		*p_++=0xb9400000u|((off/4)<<10)|(base<<5)|d;
	}
	void SR(){sr_=true;writeSR_=true;}
	void SetT(unsigned cond){*p_++=0x1a9f07e6u|((cond^1u)<<12);*p_++=0x330000c5u;}
	// cond is the FAILURE condition. All guards for an op share a stub.
	void Guard(unsigned cond,unsigned done){
		if(fixCount_>=sizeof(fix_)/sizeof(fix_[0])){failed_=true;return;}
		fix_[fixCount_++]={p_,uint8_t(cond),uint8_t(done)};*p_++=0;
	}
	static void B(uint32_t *at,uint32_t *dest){
		*at=0x14000000u|(uint32_t(dest-at)&0x03ffffffu);
	}
	static void BC(uint32_t *at,uint32_t *dest,unsigned cond){
		*at=0x54000000u|((uint32_t(dest-at)&0x7ffffu)<<5)|cond;
	}
	static unsigned Footprint(uint16_t op){
		const unsigned top=op>>12,n=(op>>8)&15,m=(op>>4)&15;
		if(Tier2Candidate(op)){
			const unsigned hi=op>>8;
			if(hi==0x80||hi==0x81||hi==0x84||hi==0x85)return 1u|(1u<<m);
			if(top==12)return 1;
			if(top==0 && Tier2Memory(op))return 1u|(1u<<n)|(1u<<m);
			if((op&0xf00f)==0x400c||(op&0xf00f)==0x400d||
				(op&0xf00f)==0x0007||(op&0xf00e)==0x200e)return (1u<<n)|(1u<<m);
			return 1u<<n;
		}
		if(top==9||top==13)return 1u<<n;
		if(top>=1 && top<=6 && top!=4)return (1u<<n)|(1u<<m);
		if(top==4||top==7||top==14||(op&0xf0ff)==0x0029)return 1u<<n;
		if((op&0xff00)==0x8800||top==12)return 1;
		return 0;
	}
	void ResolveAddress(unsigned size,bool store,unsigned done){
		// P4 is deliberately interpreted, not masked to ordinary RAM.
		*p_++=0x531d7e08u; // lsr w8,w16,#29
		*p_++=0x71001d1fu;Guard(0,done);
		*p_++=0x12007207u; // and w7,w16,#AM
		if(size){*p_++=size==1?0x720000ffu:0x720004ffu;Guard(1,done);}
		*p_++=0x53107ce8u; // lsr w8,w7,#16
		const unsigned map=store?2:size==2?1:0;
		unsigned mapBase=mapReg_[map];
		if(mapBase==31){LdrX(17,MemBase(),8*map);mapBase=17;}
		*p_++=0xf8687800u|(mapBase<<5)|17; // ldr x17,[xMap,x8,lsl #3]
		*p_++=0xf100223fu; // handler < 8
		if(extended_ && !store && size==2){
			// Direct mapped long reads fall through. The unusual
			// handler page enters an out-of-line rescue after RET.
			if(idleCount_>=CompactMaxOps){failed_=true;return;}
			uint32_t *at=p_++;*at=0;
			idle_[idleCount_++]={at,p_,uint8_t(done)};
		}else Guard(3,done);
		*p_++=0x12003ce7u; // page offset
		if(!size)*p_++=0x520000e7u; // byte lane swap
	}
	bool EmitExtended(uint16_t op,unsigned done){
		const unsigned top=op>>12,n=(op>>8)&15;
		const uint32_t opPC=pc_+2*done;
		if(PcRelative(op)){
			const bool word=top==9;
			const uint32_t ea=(word?opPC+4:(opPC+4)&~3u)+(op&255)*(word?2:4);
			Imm32(16,ea);
			if(top==12){
				*p_++=0xb9000010u|(EABase()<<5);
				*p_++=Mov(W(0),16);return true;
			}
			ResolveAddress(word?1:2,false,done);
			*p_++=0xb9000010u|(EABase()<<5);
			const unsigned d=W(n);
			*p_++=(word?0x78e76a20u:0xb8676a20u)|d;
			if(!word)*p_++=0x13804000u|(d<<16)|(d<<5)|d;
			return true;
		}
		if(IsDT(op)){
			// Check the live DATA word before decrementing. This preserves
			// both MMIO lookahead and the original DT/BF busy-loop burn.
			if(!global_ || !global_->fastDT[done]){
			Imm32(16,(opPC+2)&0x1fffffffu);
			ResolveAddress(1,false,done);
			*p_++=0x78676a28u; // ldrh w8,[x17,x7]
			Imm(6,0x8bfd);
			*p_++=0x6b06011fu;Guard(0,done);
			}
			const unsigned d=R(n);W(n);SR();
			*p_++=0x71000400u|(d<<5)|d;SetT(0);return true;
		}
		return false;
	}
	uint32_t *EmitTerminal(uint16_t op,const uint16_t *ops,unsigned body,unsigned count){
		uint32_t *notTaken=NULL;
		if(Conditional(op)){
			sr_=true;
			*p_++=0x720000bfu; // tst w5,#1
			notTaken=p_++;
		}
		if(Dynamic(op)){
			if(op==0x000b){if(resident_)*p_++=Mov(16,2);else LdrW(16,MemBase(),60);}
			else *p_++=Mov(16,R((op>>8)&15));
			*p_++=0x7200021fu;Guard(1,body); // odd target: original handler
		}else Imm32(16,StaticTarget(op,pc_+body*2));
		// Capture the target BEFORE the delay slot changes its source.
		// All accepted slots are register-only and do not access EA/PR.
		if(global_ && Delayed(op) && (MemoryOp(ops[body+1])||Tier2Memory(ops[body+1]))){
			// Resolve the slot before committing the branch. On a guard
			// exit the original branch+slot can be executed exactly.
			*p_++=0xb90106d0u; // str w16,[x22,#260]
			if(!Emit(ops[body+1],body))failed_=true;
			LdrW(16,MemBase(),260);
			if(!SlotWritesEA(ops[body+1]))*p_++=0xb9000010u|(EABase()<<5);
		}else{
			*p_++=0xb9000010u|(EABase()<<5);
			if(Delayed(op) && !Emit(ops[body+1],body+1))failed_=true;
		}
		return notTaken;
	}
	bool EmitMemory(uint16_t op,unsigned done){
		const unsigned top=op>>12,n=(op>>8)&15,m=(op>>4)&15,f=op&15;
		const bool store=top==1||top==2,pre=store&&top==2&&f>=4,
			post=!store&&top==6&&f>=4;
		const unsigned size=(top==1||top==5)?2:f&3,bytes=1u<<size;
		const unsigned a=R(store?n:m),b=store?R(m):0;
		if(top==1||top==5)*p_++=0x11000010u|(f*4<<10)|(a<<5);
		else if(pre)*p_++=0x51000010u|(bytes<<10)|(a<<5);
		else *p_++=Mov(16,a);
		ResolveAddress(size,store,done);
		if(store){
			if(resident_){
				// A resident region may contain any watched code span.
				// Exit BEFORE a write to a watched/ambiguous backing page.
				if(!global_){*p_++=0x12007208u;*p_++=0x53107d08u;}
				LdrX(6,MemBase(),64);
				*p_++=0x386868c8u;
				*p_++=0x7100011fu;Guard(1,done);
				*p_++=0x8b070231u;
			}else{
			*p_++=0x8b070231u; // add x17,x17,x7
			unsigned code=codeReg_;
			if(code==31){LdrX(4,MemBase(),24);code=4;}
			*p_++=0xcb000228u|(code<<16); // sub x8,x17,xCode
			*p_++=0x91000108u|((bytes-1)<<10);
			unsigned length=sizeReg_;
			if(length==31){LdrX(4,MemBase(),32);length=4;}
			*p_++=0x91000006u|(length<<5)|((bytes-1)<<10);
			*p_++=0xeb06011fu;Guard(3,done);
			}
			unsigned value=b;
			if(size==2){*p_++=0x13804006u|(b<<16)|(b<<5);value=6;}
			*p_++=(size==0?0x39000220u:size==1?0x79000220u:0xb9000220u)|value;
			if(audit_ && !resident_){
				// Map-normalized address, including aliases. No C calls.
				*p_++=0x12007208u; // and w8,w16,#AM
				*p_++=0x53107d08u; // lsr w8,w8,#16
				LdrX(6,MemBase(),64);
				*p_++=0x386868c8u; // ldrb w8,[x6,x8]
				if(watchCount_>=CompactMaxOps)return false;
				uint32_t *at=p_++;*at=0;watch_[watchCount_++]={at,p_};
			}
			if(pre){W(n);*p_++=Mov(a,16);}
			else *p_++=0xb9000010u|(EABase()<<5); // EA
		}else{
			if(!post)*p_++=0xb9000010u|(EABase()<<5);
			const unsigned d=W(n);
			*p_++=(size==0?0x38e76a20u:size==1?0x78e76a20u:0xb8676a20u)|d;
			if(size==2)*p_++=0x13804000u|(d<<16)|(d<<5)|d;
			if(post&&n!=m){W(m);*p_++=0x11000000u|(bytes<<10)|(a<<5)|a;}
		}
		return true;
	}
	void GlobalStore(unsigned size,unsigned value,unsigned done){
		// ResolveAddress left w8 as the normalized page index.
		LdrX(6,MemBase(),64);
		*p_++=0x386868c8u; // ldrb w8,[x6,x8]
		*p_++=0x7100011fu;Guard(1,done);
		*p_++=0x8b070231u;
		if(size==2){*p_++=0x13804006u|(value<<16)|(value<<5);value=6;}
		*p_++=(size==0?0x39000220u:size==1?0x79000220u:0xb9000220u)|value;
	}
	bool EmitTier2(uint16_t op,unsigned done){
		const unsigned top=op>>12,hi=op>>8,n=(op>>8)&15,m=(op>>4)&15,f=op&15,k=op&0xf0ff;
		if(Tier2Memory(op)){
			bool store=false,stack=false;unsigned size=0,value=0,dest=0;
			if(k==0x4022||k==0x4026){
				stack=true;store=k==0x4022;size=2;value=2;dest=2;
				if(store)*p_++=0x51001010u|(R(n)<<5);
				else *p_++=Mov(16,R(n));
			}else if(top==8){
				store=hi==0x80||hi==0x81;size=hi&1;value=R(0);
				if(!store)dest=W(0);
				*p_++=0x11000010u|((f<<size)<<10)|(R(m)<<5);
			}else if(top==0){
				store=f<8;size=f&3;value=store?R(m):0;
				if(!store)dest=W(n);
				*p_++=0x0b000010u|(R(0)<<16)|(R(store?n:m)<<5);
			}else{
				store=hi<0xc4;size=hi&3;value=R(0);
				if(!store)dest=W(0);
				LdrW(16,MemBase(),252);
				*p_++=0x11000210u|(((op&255)<<size)<<10);
			}
			ResolveAddress(size,store,done);
			if(store)GlobalStore(size,value,done);
			else{
				*p_++=(size==0?0x38e76a20u:size==1?0x78e76a20u:0xb8676a20u)|dest;
				if(size==2)*p_++=0x13804000u|(dest<<16)|(dest<<5)|dest;
			}
			*p_++=0xb9000010u|(EABase()<<5);
			if(stack){W(n);if(store)*p_++=Mov(R(n),16);
				else *p_++=0x11001000u|(R(n)<<5)|R(n);}
			return true;
		}
		if(k==0x002a){*p_++=Mov(W(n),2);return true;}
		if(k==0x402a){*p_++=Mov(2,R(n));return true;}
		if(k==0x001a){LdrW(W(n),MemBase(),256);return true;}
		if(k==0x401a){*p_++=0xb90102c0u|R(n);return true;}
		if(top==4 && (f==12||f==13)){
			const unsigned a=R(n),b=R(m);W(n);
			*p_++=0x7100001fu|(b<<5); // cmp wB,#0
			uint32_t *negative=p_++;
			*p_++=0x1ac02000u|(b<<16)|(a<<5)|a; // lslv
			uint32_t *end=p_++;
			BC(negative,p_,11); // lt
			*p_++=0x4b0003e6u|(b<<16); // neg w6,wB
			*p_++=0x720010dfu; // tst w6,#31
			uint32_t *zero=p_++;
			*p_++=(f==12?0x1ac02800u:0x1ac02400u)|(6<<16)|(a<<5)|a;
			uint32_t *end2=p_++;
			BC(zero,p_,0);
			if(f==12)*p_++=0x131f7c00u|(a<<5)|a; // asr #31
			else *p_++=Mov(a,31);
			B(end,p_);B(end2,p_);return true;
		}
		if((top==0&&f==7)||(top==2&&(f==14||f==15))){
			unsigned a=R(n),b=R(m);
			if(top==2){
				const uint32_t ext=f==15?0x13003c00u:0x53003c00u;
				*p_++=ext|(a<<5)|6;*p_++=ext|(b<<5)|7;a=6;b=7;
			}
			*p_++=0x1b007c06u|(b<<16)|(a<<5); // mul w6,wA,wB
			*p_++=0xb90102c6u; // str w6,[x22,#256]
			return true;
		}
		return false;
	}
public:
	explicit CompactEmitter(uint32_t *p):p_(p),reg_{},used_(0),dirty_(0),
		sr_(false),writeSR_(false),extended_(false),audit_(false),repeat_(false),resident_(false),pc_(0),global_(NULL),maps_(0),mapReg_{31,31,31},codeReg_(31),sizeReg_(31),
		fix_{},idle_{},watch_{},fixCount_(0),idleCount_(0),watchCount_(0),entrySR_(NULL),failed_(false){}
	uint32_t *End()const{return p_;}
	bool Wide()const{return resident_||repeat_||__builtin_popcount(unsigned(used_))>7;}
	bool Build(const uint16_t *ops,unsigned count,uint32_t pc=0,unsigned terminal=~0u,bool extended=false,bool audit=false,bool repeat=false){
		if(!count||count>CompactMaxOps)return false;
		extended_=extended;pc_=pc;audit_=audit;repeat_=repeat;
		const bool branch=terminal<count;
		const unsigned body=branch?terminal:count;
		if(repeat_ && (!branch || Dynamic(ops[body]) || Call(ops[body]) ||
			StaticTarget(ops[body],pc+body*2)!=pc))return false;
		if(branch){
			if(!extended || !Terminal(ops[body]) ||
				count!=body+1+unsigned(Delayed(ops[body])))return false;
			if(Delayed(ops[body]) && (!SafeSlot(ops[body+1]) || !(pc+body*2+2)))return false;
			if((ops[body]>>12)==10 && (ops[body]&0xfff)==0xffe)return false;
			if(Dynamic(ops[body])&&ops[body]!=0x000b)used_|=1u<<((ops[body]>>8)&15);
		}
		for(unsigned i=0;i<count;i++){
			if(branch && i==body)continue;
			const uint16_t op=ops[i];
			if(!(extended?ExtendedCandidate(op):Candidate(op)))return false;
			used_|=Footprint(op);
			if(MemoryOp(op)){
				unsigned t=op>>12,f=op&15;
				// Stores keep the existing host-span overlap guard on every
				// lap. A write to this block exits before modifying it.
				maps_|=(t==1||t==2)?4:(t==5||(f&3)==2)?2:1;
			}
			if((op>>12)==9 || IsDT(op))maps_|=1;
			if((op>>12)==13)maps_|=2;
		}
		static const unsigned host[]={9,10,11,12,13,14,15,23,24,25,26,27,28};
		if(__builtin_popcount(unsigned(used_))>sizeof(host)/sizeof(host[0]))return false;
		unsigned h=0;
		*p_++=0xd503245fu;
		for(unsigned g=0;g<16;g++)if(used_&(1u<<g)){
			reg_[g]=host[h++];*p_++=0xb9400000u|(RegBase()<<5)|(g<<10)|reg_[g];
		}
		// Leaf blocks use the normal x0-x3 ABI and unused caller-saved
		// bindings as map constants. Only large footprints need the
		// callee-saved wide trampoline. A missing map constant is
		// loaded from the context in the corresponding memory op.
		for(unsigned i=0;i<3;i++)if(maps_&(1u<<i)){
			mapReg_[i]=Wide()?i:h<7?host[h++]:31;
			if(mapReg_[i]!=31)LdrX(mapReg_[i],MemBase(),8*i);
		}
		if(maps_&4){
			codeReg_=Wide()?3:h<7?host[h++]:31;
			sizeReg_=Wide()?4:h<7?host[h++]:31;
			if(codeReg_!=31)LdrX(codeReg_,MemBase(),24);
			if(sizeReg_!=31)LdrX(sizeReg_,MemBase(),32);
		}
		// Reserved prologue instruction patched iff SR is used.
		entrySR_=p_++;*entrySR_=0xd503201fu;
		if(repeat_)LdrW(29,MemBase(),40);
		uint32_t *loopHead=p_;
		for(unsigned i=0;i<body;i++)if(!Emit(ops[i],i))return false;
		uint32_t *notTaken=branch?EmitTerminal(ops[body],ops,body,count):NULL;
		if(failed_)return false;
		if(sr_)*entrySR_=0xb9400005u|(SRBase()<<5);
		// The normal exit falls through to the common epilogue.
		// Failure and not-taken stubs are after RET, off the hot path.
		const unsigned result=Wide()?0:6;
		if(repeat_){
			// Completed taken lap: stay native with live GPR/SR bindings.
			// Last lap returns normally; guards retain earlier completed laps.
			*p_++=0x710007bdu; // subs w29,w29,#1
			uint32_t *back=p_++;BC(back,loopHead,1);
			*p_++=0x110007bdu; // add w29,w29,#1 (exclude final lap)
		}
		Imm(result,branch?ExitTaken|count:count);
		uint32_t *epilogue=p_;
		for(unsigned g=0;g<16;g++)if(dirty_&(1u<<g))
			*p_++=0xb9000000u|(RegBase()<<5)|(g<<10)|reg_[g];
		if(writeSR_)*p_++=0xb9000005u|(SRBase()<<5);
		if(repeat_){
			LdrW(8,MemBase(),40);
			*p_++=0x4b1d0108u; // sub w8,w8,w29
			*p_++=0x2a082800u; // orr w0,w0,w8,lsl #10
		}
		if(!Wide())*p_++=Mov(0,result);
		*p_++=0xd65f03c0u;
		if(notTaken){
			BC(notTaken,p_,(ops[body]&0x0200)?1:0);
			Imm(result,ExitNotTaken|(body+1));uint32_t *jump=p_++;B(jump,epilogue);
		}
		// Resume ordinary reads with the live validated idle page
		// pointer in x17. Do not rescue the exact idle address.
		for(unsigned i=0;i<idleCount_;i++){
			const auto &r=idle_[i];
			BC(r.at,p_,3); // b.lo handler
			*p_++=0x7130011fu;Guard(1,r.done); // cmp w8,#0xc00
			LdrX(17,MemBase(),48);
			*p_++=0xf100023fu;Guard(0,r.done);
			LdrW(8,MemBase(),56);
			*p_++=0x6b0800ffu;Guard(0,r.done);
			uint32_t *jump=p_++;B(jump,r.resume);
		}
		if(failed_)return false;
		uint32_t *stub[CompactMaxOps]={};
		for(unsigned i=0;i<fixCount_;i++){
			const unsigned d=fix_[i].done;
			if(!stub[d]){stub[d]=p_;Imm(result,d);uint32_t *jump=p_++;B(jump,epilogue);}
			BC(fix_[i].at,stub[d],fix_[i].cond);
		}
		for(unsigned i=0;i<watchCount_;i++){
			const auto &r=watch_[i];
			*r.at=0x35000008u|((uint32_t(p_-r.at)&0x7ffffu)<<5); // cbnz w8
			LdrX(6,MemBase(),72);
			*p_++=0xf86878d1u; // ldr x17,[x6,x8,lsl #3]
			*p_++=0x91000631u; // add x17,x17,#1
			*p_++=0xf82878d1u; // str x17,[x6,x8,lsl #3]
			uint32_t *jump=p_++;B(jump,r.resume);
		}
		return true;
	}
	// Resident C-3 regions share the same audited arithmetic/memory
	// emitter, but no helper calls or per-block hot GPR/SR round trip.
	static constexpr unsigned ResidentHotMask=0x801f,ResidentMaxWords=128*CompactMaxOps+512;
	static bool ResidentEligible(const uint16_t *ops,unsigned count,unsigned body){
		unsigned mask=0;
		for(unsigned i=0;i<count;i++){
			if(i==body && body<count){
				if(Dynamic(ops[i]) && ops[i]!=0x000b)mask|=1u<<((ops[i]>>8)&15);
			}else mask|=Footprint(ops[i]);
		}
		return __builtin_popcount(mask&~ResidentHotMask)<=7;
	}
	bool BuildResident(const uint16_t *ops,unsigned count,uint32_t pc,unsigned body,
			const uint32_t *pcs,unsigned nPC,ResidentFixup *edges,unsigned &nEdge,
			bool hasPrediction=false,uint32_t prediction=0,GlobalBuild *global=NULL){
		if(!count||count>CompactMaxOps||!ResidentEligible(ops,count,body))return false;
		resident_=extended_=true;pc_=pc;global_=global;
		const bool terminal=body<count;
		if(terminal && (!Terminal(ops[body]) ||
			count!=body+1+unsigned(Delayed(ops[body])) ||
			(Delayed(ops[body])&&!(global?GlobalSafeSlot(ops[body+1]):SafeSlot(ops[body+1])))))return false;
		for(unsigned i=0;i<count;i++){
			if(terminal && i==body){
				if(Dynamic(ops[i]) && ops[i]!=0x000b)used_|=1u<<((ops[i]>>8)&15);
			}else{
				if(!(global?GlobalCandidate(ops[i]):ExtendedCandidate(ops[i])))return false;
				used_|=Footprint(ops[i]);
			}
		}
		auto member=[&](uint32_t target){
			for(unsigned i=0;i<nPC;i++)if(pcs[i]==target)return true;
			return false;
		};
		auto reason=[&](unsigned why){
			if(global){Imm(6,why);*p_++=0xb900fac6u;} // str w6,[x22,#248]
		};
		auto cold=[&](uint32_t target,unsigned why=2){Imm32(16,target);reason(why);*p_++=0xd65f03c0u;};
		auto edge=[&](uint32_t target){
			if(global){
				if(global->count>=3){failed_=true;return;}
				global->patch[global->count]=p_;global->target[global->count++]=target;
				*p_++=0x14000001u; // initially branch to its own cold tail
				cold(target);
			}else if(member(target)){edges[nEdge++]={p_,target,false};*p_++=0x14000000u;}
			else cold(target);
		};
		// Dynamic PC -> guarded global entry. Collisions/misses are safe.
		auto dynamicEdge=[&](){
			*p_++=0x53017e08u; // lsr w8,w16,#1
			*p_++=0x12003108u; // and w8,w8,#8191
			LdrX(17,MemBase(),240);
			*p_++=0x8b081231u; // add x17,x17,x8,lsl #4
			*p_++=0xb9400226u;
			*p_++=0x6b1000dfu; // cmp w6,w16
			uint32_t *miss=p_++;
			LdrX(17,17,8);
			uint32_t *empty=p_++;
			*p_++=0xd61f0220u; // br x17
			BC(miss,p_,1);
			*empty=0xb4000011u|((uint32_t(p_-empty)&0x7ffffu)<<5);
			reason(2);*p_++=0xd65f03c0u;
		};
		auto flush=[&](){
			for(unsigned g=0;g<16;g++)if((dirty_&~ResidentHotMask)&(1u<<g))
				*p_++=0xb9000000u|(19<<5)|(g<<10)|reg_[g];
		};
		auto debit=[&](unsigned opsDone,unsigned cost,bool complete){
			if(opsDone)*p_++=0x11000063u|(opsDone<<10); // add w3,w3,#ops
			if(cost)*p_++=0x510003bdu|(cost<<10); // sub w29,w29,#cost
			if(complete)*p_++=0x11000484u; // add w4,w4,#1
		};
		uint32_t *rasAdr=NULL;uint32_t rasGuest=0;
		auto rasPush=[&](uint32_t ret){
			if(!global && !member(ret))return;
			*p_++=0x7100201fu; // cmp w0,#8
			uint32_t *full=p_++;
			*p_++=0x910182d1u; // add x17,x22,#96
			*p_++=0x8b001231u; // add x17,x17,x0,lsl #4
			*p_++=0xb9000222u; // str w2,[x17]
			if(global){rasAdr=p_;rasGuest=ret;}
			else edges[nEdge++]={p_,ret,true};
			*p_++=0x10000010u; // ADR x16, continuation
			*p_++=0xf9000630u; // str x16,[x17,#8]
			*p_++=0x11000400u; // add w0,w0,#1
			BC(full,p_,2);
		};
		auto rasReturn=[&](){
			uint32_t *empty=p_++;
			*p_++=0x51000400u; // sub w0,w0,#1
			*p_++=0x910182d1u;
			*p_++=0x8b001231u;
			*p_++=0xb9400226u; // ldr w6,[x17]
			*p_++=0x6b1000dfu; // cmp w6,w16
			uint32_t *miss=p_++;
			*p_++=0xf9400631u; // ldr x17,[x17,#8]
			*p_++=0xd61f0220u; // br x17
			*empty=0x34000000u|((uint32_t(p_-empty)&0x7ffffu)<<5);
			BC(miss,p_,1);
			if(global)dynamicEdge();else *p_++=0xd65f03c0u;
		};
		// All incoming B/BR/RAS edges hit the same validity and cycle gate.
		*p_++=0xd50324dfu;
		uint32_t *stampLoad=NULL,*epochMiss=NULL,*genMiss=NULL;
		if(global){
			stampLoad=p_++; // ldr x17, literal address of pinned stamp
			*p_++=0xa9401e26u; // ldp x6,x7,[x17]
			if(global->trusted)*p_++=0xeb1400dfu; // cmp x6,x20 (trusted fence)
			else{LdrX(8,MemBase(),264);*p_++=0xeb0800dfu;} // invocation-local unknown backing
			epochMiss=p_++;
			LdrX(17,17,16);LdrX(17,17,0);
			*p_++=0xeb1100ffu; // cmp x7,x17 (live generation)
			genMiss=p_++;
		}
		unsigned prefix[CompactMaxOps+1]={};
		for(unsigned i=0;i<body;i++)prefix[i+1]=prefix[i]+(global?GlobalExtra(ops[i]):0);
		const unsigned worst=count+prefix[body]+(terminal?ExtraCycles(ops[body]):0);
		*p_++=0x710003bfu|(worst<<10); // cmp w29,#maxCost
		uint32_t *budget=p_++;
		for(unsigned g=0,h=9;g<16;g++){
			if(g<5)reg_[g]=23+g;
			else if(g==15)reg_[g]=28;
			else if(used_&(1u<<g)){
				reg_[g]=h++;
				LdrW(reg_[g],19,g*4);
			}
		}
		for(unsigned i=0;i<body;i++)if(!Emit(ops[i],i))return false;
		uint32_t *notTaken=terminal?EmitTerminal(ops[body],ops,body,count):NULL;
		if(failed_)return false;
		flush();debit(count,worst,true);
		if(!terminal){
			Imm32(1,pc+count*2);edge(pc+count*2);
		}else{
			const uint16_t op=ops[body];
			const uint32_t next=pc+(body+1)*2;
			if(Dynamic(op)){
				// Dynamic target is captured before the register-only slot.
				*p_++=Mov(1,16);
				if(Call(op))Imm32(2,next+2);
				if(op==0x000b)rasReturn();
				else if(global){
					if(Call(op)){*p_++=Mov(8,16);rasPush(next+2);*p_++=Mov(16,8);}
					dynamicEdge();
				}else{
					if(hasPrediction && member(prediction)){
						Imm32(6,prediction);
						*p_++=0x6b06021fu; // cmp w16,w6, actual captured target
						uint32_t *miss=p_++;
						if(Call(op))rasPush(next+2);
						edge(prediction);
						BC(miss,p_,1);
					}
					// Misses train a bounded monomorphic prediction on exit.
					// Actual guest target and PR have already been committed.
					Imm32(6,(pc+body*2)|1u);
					*p_++=0xb900e2c6u; // str w6,[x22,#224]
					*p_++=0xb900e6d0u; // str w16,[x22,#228]
					*p_++=0xd65f03c0u;
				}
			}else{
				const uint32_t target=StaticTarget(op,pc+body*2);
				Imm32(1,Delayed(op)?target:next);
				if(Call(op)){Imm32(2,next+2);if(global||member(target))rasPush(next+2);}
				edge(target);
			}
			if(notTaken){
				BC(notTaken,p_,(op&0x0200)?1:0);
				flush();debit(body+1,body+1+prefix[body],true);Imm32(1,next);edge(next);
			}
		}
		BC(budget,p_,3);cold(pc,3);
		for(unsigned i=0;i<idleCount_;i++){
			const auto &r=idle_[i];
			BC(r.at,p_,3);
			*p_++=0x7130011fu;Guard(1,r.done);
			LdrX(17,MemBase(),48);
			*p_++=0xf100023fu;Guard(0,r.done);
			LdrW(8,MemBase(),56);
			*p_++=0x6b0800ffu;Guard(0,r.done);
			uint32_t *jump=p_++;B(jump,r.resume);
		}
		if(failed_)return false;
		uint32_t *stub[CompactMaxOps]={};
		for(unsigned i=0;i<fixCount_;i++){
			const unsigned d=fix_[i].done;
			if(!stub[d]){
				stub[d]=p_;flush();debit(d,d+prefix[d],false);
				if(d)Imm32(1,pc+d*2);
				cold(pc+d*2,4);
			}
			BC(fix_[i].at,stub[d],fix_[i].cond);
		}
		if(global){
			if(rasAdr){
				const int32_t bytes=int32_t(p_-rasAdr)*4;
				*rasAdr=0x10000010u|((uint32_t(bytes)&3)<<29)|(((uint32_t(bytes)>>2)&0x7ffffu)<<5);
				*p_++=0xd50324dfu; // RAS indirect branch target
				edge(rasGuest);
			}
			BC(epochMiss,p_,1);BC(genMiss,p_,1);cold(pc,1);
			if((p_-stampLoad)&1)*p_++=0xd503201fu;
			*stampLoad=0x58000011u|((uint32_t(p_-stampLoad)&0x7ffffu)<<5);
			const uint64_t ptr=(uintptr_t)global->stamp;
			*p_++=uint32_t(ptr);*p_++=uint32_t(ptr>>32);
		}
		return !failed_;
	}
private:
	bool Emit(uint16_t op,unsigned done){
		const unsigned top=op>>12,n=(op>>8)&15,m=(op>>4)&15,f=op&15,imm=op&255;
		unsigned a,b,d;uint32_t base=0;unsigned cond=0;bool test=false;
		if(global_ && Tier2Candidate(op))return EmitTier2(op,done);
		if(extended_ && (PcRelative(op)||IsDT(op)))return EmitExtended(op,done);
		if(op==0x0009)return true;
		if(op==0x0008 || op==0x0018){
			SR();Imm(6,op==0x0018);*p_++=0x330000c5u;return true;
		}
		if((op&0xf0ff)==0x0029){
			sr_=true;d=W(n);*p_++=0x53000000u|(5<<5)|d;return true; // MOVT
		}
		if(top==0xe){d=W(n);Imm(d,int8_t(imm));return true;}
		if(top==7){d=R(n);W(n);int v=int8_t(imm);
			*p_++=(v<0?0x51000000u:0x11000000u)|(unsigned(v<0?-v:v)<<10)|(d<<5)|d;return true;}
		if(top==2 && f>=8 && f<=11){
			a=R(n);b=R(m);
			if(f==8){SR();*p_++=0x6a00001fu|(b<<16)|(a<<5);SetT(0);return true;}
			base=f==9?0x0a000000u:f==10?0x4a000000u:0x2a000000u;
			d=W(n);*p_++=base|(b<<16)|(a<<5)|d;return true;
		}
		if(top==2 && f==13){
			a=R(n);b=R(m);d=W(n);
			*p_++=0x13804000u|(a<<16)|(b<<5)|d;return true; // XTRCT
		}
		if(top==3){
			switch(f){
				case 0:cond=0;test=true;break;case 2:cond=2;test=true;break;
				case 3:cond=10;test=true;break;case 6:cond=8;test=true;break;
				case 7:cond=12;test=true;break;case 8:base=0x4b000000u;break;
				case 12:base=0x0b000000u;break;default:return false;
			}
			a=R(n);b=R(m);
			if(test){SR();*p_++=0x6b00001fu|(b<<16)|(a<<5);SetT(cond);}
			else{d=W(n);*p_++=base|(b<<16)|(a<<5)|d;}
			return true;
		}
		if(MemoryOp(op))return EmitMemory(op,done);
		if(top==6 && (f==3||f==7||f==8||f==9||f==11||f>=12)){
			a=R(m);d=W(n);
			switch(f){
				case 3:*p_++=Mov(d,a);break;
				case 7:*p_++=0x2a2003e0u|(a<<16)|d;break;
				case 8: // SWAP.B: preserve the high 16 bits.
					*p_++=0x5ac00406u|(a<<5); // REV16 w6,wA
					if(d!=a)*p_++=Mov(d,a);
					*p_++=0x33003cc0u|d;break; // BFI wD,w6,#0,#16
				case 9:*p_++=0x13804000u|(a<<16)|(a<<5)|d;break;
				case 11:*p_++=0x4b0003e0u|(a<<16)|d;break;
				default:*p_++=(f>=14?0x13000000u:0x53000000u)|((f&1?15u:7u)<<10)|(a<<5)|d;break;
			}
			return true;
		}
		if(top==4 && (imm==0x11||imm==0x15)){
			a=R(n);SR();*p_++=0x7100001fu|(a<<5);
			SetT(imm==0x11?10:12);return true; // CMP/PZ, CMP/PL
		}
		if(top==4 && (imm==0||imm==1||imm==0x20||imm==0x21)){
			d=R(n);W(n);SR();
			const bool left=(imm&1)==0;
			const unsigned bit=left?31:0;
			*p_++=0x53000006u|(bit<<16)|(bit<<10)|(d<<5);
			*p_++=0x330000c5u; // insert outgoing bit into T
			*p_++=(imm==0x21?0x13000000u:0x53000000u)|
				((left?31u:1u)<<16)|((left?30u:31u)<<10)|(d<<5)|d;
			return true;
		}
		if(top==4 && (imm==8||imm==9||imm==0x18||imm==0x19||imm==0x28||imm==0x29)){
			unsigned sh=imm<0x10?2:imm<0x20?8:16;
			d=R(n);W(n);
			*p_++=0x53000000u|((imm&1?sh:32-sh)<<16)|((imm&1?31:31-sh)<<10)|(d<<5)|d;
			return true;
		}
		if((op&0xff00)==0x8800 || (op&0xff00)==0xc800 ||
			(op&0xff00)==0xc900 || (op&0xff00)==0xca00){
			a=R(0);Imm(6,(op&0xff00)==0x8800?int8_t(imm):int(imm));
			if((op&0xff00)==0x8800 || (op&0xff00)==0xc800){
				SR();*p_++=((op&0xff00)==0x8800?0x6b06001fu:0x6a06001fu)|(a<<5);SetT(0);
			}else{W(0);*p_++=((op&0xff00)==0xc900?0x0a060000u:0x4a060000u)|(a<<5)|a;}
			return true;
		}
		return false;
	}
};
}
#endif
#endif
