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

// Fixed bindings allow a single union-of-writes epilogue, even when a
// guard exits before an instruction that would otherwise overwrite a
// register: every used register is initialized at entry.
class CompactEmitter {
	uint32_t *p_;
	uint8_t reg_[16];
	uint16_t used_,dirty_;
	bool sr_,writeSR_,extended_,audit_,repeat_;
	uint32_t pc_;
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
			Imm32(16,(opPC+2)&0x1fffffffu);
			ResolveAddress(1,false,done);
			*p_++=0x78676a28u; // ldrh w8,[x17,x7]
			Imm(6,0x8bfd);
			*p_++=0x6b06011fu;Guard(0,done);
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
			if(op==0x000b)LdrW(16,MemBase(),60);
			else *p_++=Mov(16,R((op>>8)&15));
			*p_++=0x7200021fu;Guard(1,body); // odd target: original handler
		}else Imm32(16,StaticTarget(op,pc_+body*2));
		// Capture the target BEFORE the delay slot changes its source.
		// All accepted slots are register-only and do not access EA/PR.
		*p_++=0xb9000010u|(EABase()<<5);
		if(Delayed(op) && !Emit(ops[body+1],body+1))failed_=true;
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
			*p_++=0x8b070231u; // add x17,x17,x7
			unsigned code=codeReg_;
			if(code==31){LdrX(4,MemBase(),24);code=4;}
			*p_++=0xcb000228u|(code<<16); // sub x8,x17,xCode
			*p_++=0x91000108u|((bytes-1)<<10);
			unsigned length=sizeReg_;
			if(length==31){LdrX(4,MemBase(),32);length=4;}
			*p_++=0x91000006u|(length<<5)|((bytes-1)<<10);
			*p_++=0xeb06011fu;Guard(3,done);
			unsigned value=b;
			if(size==2){*p_++=0x13804006u|(b<<16)|(b<<5);value=6;}
			*p_++=(size==0?0x39000220u:size==1?0x79000220u:0xb9000220u)|value;
			if(audit_){
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
public:
	explicit CompactEmitter(uint32_t *p):p_(p),reg_{},used_(0),dirty_(0),
		sr_(false),writeSR_(false),extended_(false),audit_(false),repeat_(false),pc_(0),maps_(0),mapReg_{31,31,31},codeReg_(31),sizeReg_(31),
		fix_{},idle_{},watch_{},fixCount_(0),idleCount_(0),watchCount_(0),entrySR_(NULL),failed_(false){}
	uint32_t *End()const{return p_;}
	bool Wide()const{return repeat_||__builtin_popcount(unsigned(used_))>7;}
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
private:
	bool Emit(uint16_t op,unsigned done){
		const unsigned top=op>>12,n=(op>>8)&15,m=(op>>4)&15,f=op&15,imm=op&255;
		unsigned a,b,d;uint32_t base=0;unsigned cond=0;bool test=false;
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
