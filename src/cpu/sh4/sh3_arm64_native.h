// Experimental SH-3 ARM64 spans. Native integer operations and guarded
// direct RAM access. Opt-in closed loops also translate their back edge
// and pure delay slot. Devices remain in the core. Independently written.
#ifndef FBNEO_SH3_ARM64_NATIVE_H
#define FBNEO_SH3_ARM64_NATIVE_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#if defined(__aarch64__) && defined(__linux__) && !defined(__AARCH64EB__)
#include <sys/mman.h>
#include <unistd.h>
#define SH3_NATIVE 1
#else
#define SH3_NATIVE 0
#endif
#if SH3_NATIVE
namespace Sh3Native {
static const unsigned MaxOps=64, MinOps=4, MaxWords=80*MaxOps+32;
static const uint32_t Ret=0xd65f03c0u;
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
// Conservative register footprint for the audited Candidate instruction set.
inline unsigned Registers(uint16_t op) {
	unsigned t=op>>12,n=(op>>8)&15,m=(op>>4)&15;
	if(t>=1 && t<=6 && t!=4)return (1u<<n)|(1u<<m);
	if(t==4 || t==7 || t==14 || (op&0xf0ff)==0x0029)return 1u<<n;
	if((op&0xff00)==0x8800 || t==12)return 1;
	return 0;
}
// Only a backwards edge to this exact entry; no indirect targets, calls,
// nested branches or cross-page regions. The entire region is validated.
inline bool LoopBranch(uint16_t op, unsigned body, bool &delay) {
	unsigned hi=op>>8;
	if(hi==0x89 || hi==0x8b || hi==0x8d || hi==0x8f) {
		delay=(hi&4)!=0;
		return int(body)+2+int8_t(op)==0;
	}
	if((op>>12)==10) {
		delay=true;
		int disp=int(op&0xfff);if(disp&0x800)disp-=0x1000;
		return int(body)+2+disp==0;
	}
	return false;
}
// A call-local context keeps all map lookups live. The code interval is
// a HOST address range, so stores through guest aliases cannot change an
// opcode that this invocation has already validated. No page epochs.
struct Memory {
	uint8_t *const *read, *const *readLong, *const *write;
	const void *code;
	uintptr_t codeBytes;
	uint32_t iterations, pc;
	// V2 extension. V1 uses only the original first 48 bytes.
	const uint8_t *idlePage;
	uint32_t idleAddress,pr;
};
static_assert(offsetof(Memory,readLong)==8 && offsetof(Memory,write)==16 &&
	offsetof(Memory,code)==24 && offsetof(Memory,codeBytes)==32 &&
	offsetof(Memory,iterations)==40 && offsetof(Memory,pc)==44 && offsetof(Memory,idlePage)==48 &&
	offsetof(Memory,idleAddress)==56 && offsetof(Memory,pr)==60,"native memory ABI");
static inline uint32_t Mov(unsigned d,unsigned s) { return 0x2a0003e0u|(s<<16)|d; }
class Emitter {
	uint32_t *p_;
	struct Slot { unsigned guest, age; bool dirty; } slot_[7];
	unsigned age_;
	bool sr_;
	bool loop_;
	uint32_t *loopHead_;
	unsigned Acquire(unsigned g,bool load,unsigned pin=31) {
		unsigned choice=7;
		for(unsigned i=0;i<7;i++){
			if(slot_[i].guest==g){slot_[i].age=++age_;if(!load)slot_[i].dirty=true;return i+9;}
			if(i+9!=pin && (choice==7 || slot_[i].guest==16 ||
				(slot_[choice].guest!=16 && slot_[i].age<slot_[choice].age))) choice=i;
		}
		if(slot_[choice].dirty) *p_++=0xb9000000u|(slot_[choice].guest<<10)|(choice+9);
		slot_[choice]={g,++age_,!load};
		if(load) *p_++=0xb9400000u|(g<<10)|(choice+9);
		return choice+9;
	}
	unsigned R(unsigned g,unsigned pin=31){return Acquire(g,true,pin);}
	unsigned W(unsigned g,unsigned pin=31){return Acquire(g,false,pin);}
	void Imm(unsigned d,int v) {
		*p_++=(v<0?0x12800000u:0x52800000u)|(uint32_t(v<0?~v:v)<<5)|d;
	}
	void SR(){if(!sr_){*p_++=0xb9400025u;sr_=true;}}
	void SetT(unsigned cond){*p_++=0x1a9f07e6u|((cond^1u)<<12);*p_++=0x330000c5u;}
	void Flush(unsigned done) {
		for(unsigned i=0;i<7;i++)if(slot_[i].dirty)*p_++=0xb9000000u|(slot_[i].guest<<10)|(i+9);
		if(sr_)*p_++=0xb9000025u;
		if(loop_) {
			// Return completed taken iterations in bits 8+, exit index in 0..7.
			*p_++=0x53185e80u; // lsl w0,w20,#8
			Imm(6,done);*p_++=0x2a060000u; // orr w0,w0,w6
			*p_++=0xa8c153f3u; // ldp x19,x20,[sp],#16
		} else Imm(0,done);
		*p_++=Ret;
	}
	void Guard(unsigned cond,unsigned done){
		uint32_t *branch=p_++;Flush(done);
		*branch=0x54000000u|(uint32_t(p_-branch)<<5)|cond;
	}
public:
	explicit Emitter(uint32_t *p):p_(p),slot_{},age_(0),sr_(false),loop_(false),loopHead_(NULL){
		for(auto &s:slot_)s.guest=16;
		*p_++=0xd503245fu; // BTI c (NOP on older ARM64)
	}
	uint32_t *End()const{return p_;}
	void Finish(unsigned done){Flush(done);}
	void BeginLoop(unsigned regs) {
		loop_=true;
		*p_++=0xa9bf53f3u; // stp x19,x20,[sp,#-16]!
		*p_++=0xb9402873u; // ldr w19,[x3,#40] (iteration cap)
		Imm(20,0);
		for(unsigned g=0;g<16;g++)if(regs&(1u<<g)){R(g);W(g);}
		SR();
		// At most seven registers: the mapping cannot change across an edge.
		// Mark all slots dirty up front so every early exit publishes state.
		loopHead_=p_;
	}
	void EndLoop(uint16_t branch,uint16_t delayOp,unsigned body,bool delay) {
		uint32_t *notTaken=NULL;
		if((branch>>12)!=10)notTaken=p_++;
		*p_++=0xb9402c66u; // ldr w6,[x3,#44] (original virtual entry PC)
		*p_++=0xb9000046u; // str w6,[x2] (EA before the delay slot)
		if(delay)Emit(delayOp,body+1); // preflight requires a pure Candidate
		*p_++=0x11000694u; // add w20,w20,#1
		*p_++=0x71000673u; // subs w19,w19,#1
		uint32_t *back=p_++;
		*back=0x54000001u|((uint32_t(loopHead_-back)&0x7ffff)<<5); // b.ne
		Flush(64);
		if(notTaken) {
			// BT(/S) falls through for T=0; BF(/S) for T=1.
			*notTaken=((branch&0x0200)?0x37000005u:0x36000005u)|
				((uint32_t(p_-notTaken)&0x3fff)<<5);
			if(delay)Emit(delayOp,body+1);
			Flush(65);
		}
	}
	bool Emit(uint16_t op,unsigned done){
		const unsigned top=op>>12,n=(op>>8)&15,m=(op>>4)&15,f=op&15,imm=op&255;
		unsigned a,b,d;uint32_t base=0;unsigned cond=0;bool test=false;
		if(op==0x0009)return true;
		if(op==0x0008 || op==0x0018){
			SR();Imm(6,op==0x0018);*p_++=0x330000c5u;return true;
		}
		if((op&0xf0ff)==0x0029){
			SR();d=W(n);*p_++=0x53000000u|(5<<5)|d;return true; // MOVT
		}
		if(top==0xe){d=W(n);Imm(d,int8_t(imm));return true;}
		if(top==7){d=R(n);W(n);int v=int8_t(imm);
			*p_++=(v<0?0x51000000u:0x11000000u)|(unsigned(v<0?-v:v)<<10)|(d<<5)|d;return true;}
		if(top==2 && f>=8 && f<=11){
			a=R(n);b=R(m,a);
			if(f==8){SR();*p_++=0x6a00001fu|(b<<16)|(a<<5);SetT(0);return true;}
			base=f==9?0x0a000000u:f==10?0x4a000000u:0x2a000000u;
			d=W(n);*p_++=base|(b<<16)|(a<<5)|d;return true;
		}
		if(top==2 && f==13){
			a=R(n);b=R(m,a);d=W(n);
			*p_++=0x13804000u|(a<<16)|(b<<5)|d;return true; // XTRCT
		}
		if(top==3){
			switch(f){
				case 0:cond=0;test=true;break;case 2:cond=2;test=true;break;
				case 3:cond=10;test=true;break;case 6:cond=8;test=true;break;
				case 7:cond=12;test=true;break;case 8:base=0x4b000000u;break;
				case 12:base=0x0b000000u;break;default:return false;
			}
			a=R(n);b=R(m,a);
			if(test){SR();*p_++=0x6b00001fu|(b<<16)|(a<<5);SetT(cond);}
			else{d=W(n);*p_++=base|(b<<16)|(a<<5)|d;}
			return true;
		}
		if(top==1 || (top==2 && (f<=2 || (f>=4 && f<=6)))) {
			const bool pre=top==2 && f>=4;
			const unsigned size=top==1?2:f&3, bytes=1u<<size;
			a=R(n);b=R(m,a);
			if(top==1)*p_++=0x11000010u|(f*4<<10)|(a<<5);
			else if(pre)*p_++=0x51000010u|(bytes<<10)|(a<<5);
			else *p_++=Mov(16,a);
			*p_++=0x531d7e08u; // lsr w8,w16,#29
			*p_++=0x71001d1fu; // cmp w8,#7
			*p_++=0x12007207u; // and w7,w16,#AM
			*p_++=0x1a9010e7u; // csel w7,w7,w16,ne
			if(size){*p_++=size==1?0x720000ffu:0x720004ffu;Guard(0,done);}
			*p_++=0x53107ce8u; // lsr w8,w7,#16
			*p_++=0xf9400871u; // ldr x17,[x3,#16] (write map)
			*p_++=0xf8687a31u; // ldr x17,[x17,x8,lsl #3]
			*p_++=0xf100223fu;Guard(2,done); // handler exits before ANY side effect
			*p_++=0x12003ce7u; // and w7,w7,#0xffff
			if(!size)*p_++=0x520000e7u; // byte lane swap
			*p_++=0x8b070231u; // add x17,x17,x7 (host destination)
			// Reject overlap with the live code interval, including a long
			// store starting two bytes before it. Unsigned subtraction also
			// permits destinations below the interval without a second branch.
			*p_++=0xf9400c64u; // ldr x4,[x3,#24]
			*p_++=0xcb040228u; // sub x8,x17,x4
			*p_++=0x91000108u|((bytes-1)<<10);
			*p_++=0xf9401064u; // ldr x4,[x3,#32]
			*p_++=0x91000084u|((bytes-1)<<10);
			*p_++=0xeb04011fu;Guard(2,done); // cmp x8,x4; allow HS
			if(size==2){*p_++=0x13804006u|(b<<16)|(b<<5);b=6;}
			*p_++=(size==0?0x39000220u:size==1?0x79000220u:0xb9000220u)|b;
			if(pre){W(n);*p_++=Mov(a,16);}
			else *p_++=0xb9000050u; // EA is unchanged by predecrement stores
			return true;
		}
		if((top==6 && (f<=2 || (f>=4 && f<=6))) || top==5){
			// Live direct-map lookup, never bypass a handler (including
			// the CV1000 speedhack long-read page). Alignment failure
			// exits before the guest instruction has any side effects.
			const bool post=top==6 && f>=4;
			const unsigned size=top==5?2:f&3;
			a=R(m);
			if(top==5)*p_++=0x11000010u|(f*4<<10)|(a<<5);
			else *p_++=Mov(16,a);
			*p_++=0x531d7e08u; // lsr w8,w16,#29
			*p_++=0x71001d1fu; // cmp w8,#7
			*p_++=0x12007207u; // and w7,w16,#AM
			*p_++=0x1a9010e7u; // csel w7,w7,w16,ne
			if(size){*p_++=size==1?0x720000ffu:0x720004ffu;Guard(0,done);}
			*p_++=0x53107ce8u;
			*p_++=size==2?0xf9400471u:0xf9400071u; // current read map
			*p_++=0xf8687a31u; // ldr x17,[x17,x8,lsl #3]
			*p_++=0xf100223fu;Guard(2,done);
			*p_++=0x12003ce7u;
			if(!size)*p_++=0x520000e7u;
			if(!post)*p_++=0xb9000050u;
			d=W(n,a);
			*p_++=(size==0?0x38e76a20u:size==1?0x78e76a20u:0xb8676a20u)|d;
			if(size==2)*p_++=0x13804000u|(d<<16)|(d<<5)|d;
			if(post && n!=m){W(m);*p_++=0x11000000u|((1u<<size)<<10)|(a<<5)|a;}
			return true;
		}
		if(top==6 && (f==3||f==7||f==8||f==9||f==11||f>=12)){
			a=R(m);d=W(n,a);
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

class Cache {
	uint8_t *base_;
	size_t used_, payload_, page_, capacity_;
	bool denied_, packed_;
	Cache(const Cache&);
	Cache& operator=(const Cache&);
public:
	static const size_t DefaultCapacity = 8u * 1024u * 1024u;
	explicit Cache(size_t capacity = DefaultCapacity, bool packed = true)
		: base_(NULL), used_(0), payload_(0), page_(0), capacity_(capacity),
		  denied_(false), packed_(packed) {}
	~Cache() { Reset(); }
	void Reset() {
		if (base_) munmap(base_, capacity_);
		base_ = NULL; used_ = 0; payload_ = 0; page_ = 0;
		// A permission failure is sticky until the next explicit enable/init.
	}
	void Configure(size_t capacity, bool packed) {
		Reset();
		capacity_ = capacity;
		packed_ = packed;
	}
	void Enable() { denied_ = false; }
	bool Publish(const uint32_t *words, size_t count, uint32_t &offset) {
		if (!count || denied_) return false;
		if (!base_) {
			long page = sysconf(_SC_PAGESIZE);
			if (page <= 0 || !capacity_ || capacity_ % size_t(page) ||
				capacity_ > UINT32_MAX) { denied_ = true; return false; }
			page_ = size_t(page);
			void *p = mmap(NULL, capacity_, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
			if (p == MAP_FAILED) { denied_ = true; return false; }
			base_ = static_cast<uint8_t *>(p);
		}
		if (count > capacity_ / sizeof(uint32_t)) return false;
		const size_t bytes = count * sizeof(uint32_t);
		const size_t align = packed_ ? 16 : page_;
		const size_t rounded = ((bytes + align - 1) / align) * align;
		if (rounded > capacity_ - used_) return false;

		const size_t start = used_;
		const size_t end = start + rounded;
		const size_t pageStart = start / page_ * page_;
		const size_t pageEnd = ((end + page_ - 1) / page_) * page_;
		// Existing code on the partially filled page is not running here.
		// New, untouched pages already have RW permission.
		if (start != pageStart &&
			mprotect(base_ + pageStart, page_, PROT_READ | PROT_WRITE)) {
			denied_ = true;
			return false;
		}
		uint8_t *dst = base_ + start;
		memcpy(dst, words, bytes);
		__builtin___clear_cache(reinterpret_cast<char *>(dst), reinterpret_cast<char *>(dst + bytes));
		if (mprotect(base_ + pageStart, pageEnd - pageStart, PROT_READ | PROT_EXEC)) {
			// Even older spans on the tail page may no longer be executable.
			// The owning CPU must never enter the cache after this failure.
			denied_ = true;
			return false;
		}
		offset = uint32_t(start);
		used_ = end;
		payload_ += bytes;
		return true;
	}
	unsigned Run(uint32_t offset, uint32_t *r, uint32_t *sr, uint32_t *ea,
		uint8_t *const *read, uint8_t *const *readLong, uint8_t *const *write,
		const void *code = NULL, size_t codeBytes = 0,
		unsigned iterations = 0, uint32_t pc = 0) const {
		const Memory memory={read,readLong,write,code,codeBytes,iterations,pc};
		return reinterpret_cast<unsigned (*)(uint32_t*,uint32_t*,uint32_t*,const Memory*)>(base_+offset)(r,sr,ea,&memory);
	}
	bool Denied() const { return denied_; }
	size_t Used() const { return used_; }
	size_t Payload() const { return payload_; }
	size_t CapacityBytes() const { return capacity_; }
	size_t MappedCodeBytes() const { return page_ ? ((used_ + page_ - 1) / page_) * page_ : 0; }
};

}
#endif
#endif
