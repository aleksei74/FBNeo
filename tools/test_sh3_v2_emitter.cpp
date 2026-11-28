// Independent checks against the production SH3 instruction handlers.
#define main InterpreterMain
#include "test_sh3_interpreter.cpp"
#undef main
#if !SH3_NATIVE_V2
#error AArch64 Linux little-endian memfd support required
#endif
static UINT32 rng=0x245f957fu;
static UINT32 Rand(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static void Regs(UINT32 *r){for(unsigned i=0;i<16;i++)r[i]=Rand();}
static void CheckState(const UINT32 *r,UINT32 sr,UINT32 ea,unsigned done,unsigned expected,
	unsigned test,const char *what){
	if(done!=expected || memcmp(r,m_r,sizeof(m_r)) || sr!=m_sr || ea!=m_ea){
		fprintf(stderr,"%s %04x: done %u/%u SR %08x/%08x EA %08x/%08x\n",
			what,test,done,expected,sr,m_sr,ea,m_ea);
		for(unsigned i=0;i<16;i++)if(r[i]!=m_r[i])
			fprintf(stderr,"r%u %08x/%08x\n",i,r[i],m_r[i]);
		exit(1);
	}
}
static void Encodings(){
	setenv("FBNEO_CV1K_JIT_V2","0",1);Setup(10,0);
	Sh3V2::CodeCache cache;
	unsigned tested=0,guards=0;UINT64 oldBytes=0,newBytes=0;
	for(unsigned op=0;op<65536;op++){
		Check(Sh3V2::Candidate(op)==Sh3Native::Candidate(op),"candidate agreement");
		if(!Sh3V2::Candidate(op))continue;
		if(!(tested%256))cache.Reset();
		UINT32 words[Sh3Native::MaxWords],old[Sh3Native::MaxWords],offset;
		const UINT16 inst=UINT16(op);
		Sh3V2::CompactEmitter e(words);Check(e.Build(&inst,1),"one-op build");
		Sh3Native::Emitter ref(old);Check(ref.Emit(inst,0),"legacy one-op build");ref.Finish(1);
		oldBytes+=(ref.End()-old)*4;newBytes+=(e.End()-words)*4;
		Check(cache.Publish(words,e.End()-words,offset),"one-op publish");
		UINT32 regs[16],sr=Rand(),ea=Rand();
		Regs(regs);
		const unsigned top=op>>12,f=op&15,m=(op>>4)&15,n=(op>>8)&15;
		const bool mem=Sh3V2::MemoryOp(op),store=mem&&(top==1||top==2),
			pre=store&&top==2&&f>=4;
		const unsigned size=(top==1||top==5)?2:f&3,bytes=1u<<size;
		const unsigned addr=store?n:m;
		const UINT32 aliases[]={0x0c000000u,0x8c000000u,0xac000000u,0xcc000000u};
		const unsigned dst=(Rand()&0xff00u);
		if(mem)regs[addr]=aliases[tested%4]+dst+(pre?bytes:0)-((top==1||top==5)?4*f:0);
		UINT8 before[8],after[8];UINT8 *ptr=(UINT8*)data+dst;
		if(store)memcpy(before,ptr,8);
		memcpy(m_r,regs,sizeof(regs));m_sr=sr;m_ea=ea;
		m_sh4_icount=10;sh3_total_cycles=0;
		execute_one(inst);
		Check(m_sh4_icount==10 && !sh3_total_cycles,"unit-cycle op");
		if(store){memcpy(after,ptr,8);memcpy(ptr,before,8);}
		const Sh3Native::Memory maps={MemMapR,MemMapRL,MemMapW,NULL,0,0,0};
		unsigned done=cache.RunCompact(offset,regs,&sr,&ea,maps,e.Wide());
		CheckState(regs,sr,ea,done,1,op,"single");
		if(store)Check(!memcmp(after,ptr,8),"store bytes/endian");
		if(mem){
			for(unsigned k=0;k<3;k++){
				Regs(regs);sr=Rand();ea=Rand();
				regs[addr]=(k==0?0x8d000100u:k==1?0xec000100u:0xfc000100u)
					+(pre?bytes:0)-((top==1||top==5)?4*f:0);
				memcpy(m_r,regs,sizeof(regs));m_sr=sr;m_ea=ea;
				done=cache.RunCompact(offset,regs,&sr,&ea,maps,e.Wide());
				CheckState(regs,sr,ea,done,0,op,"unmapped/P4 guard");++guards;
			}
			if(!store && size==2){
				Regs(regs);regs[addr]=0x8c000100u-((top==5)?4*f:0);
				memcpy(m_r,regs,sizeof(regs));m_sr=sr;m_ea=ea;
				MemMapRL[0xc00]=(UINT8*)1;
				done=cache.RunCompact(offset,regs,&sr,&ea,maps,e.Wide());
				CheckState(regs,sr,ea,done,0,op,"long-only handler guard");
				MemMapRL[0xc00]=(UINT8*)data;++guards;
			}
		}
		tested++;
	}
	printf("V2 compact single encodings: %u, MMIO/P4/long-handler guards: %u PASS\n",tested,guards);
	printf("V2 one-op payload bytes: legacy %llu compact %llu (excludes shared trampoline)\n",
		(unsigned long long)oldBytes,(unsigned long long)newBytes);
}
static void Mixed(){
	Setup(10,0);
	Sh3V2::CodeCache cache;
	unsigned wide=0;UINT64 oldBytes=0,newBytes=0;
	for(unsigned c=0;c<4096;c++){
		if(!(c%128))cache.Reset();
		UINT16 ops[64];UINT32 words[Sh3Native::MaxWords],old[Sh3Native::MaxWords],offset;
		UINT32 regs[16],sr=Rand(),ea=Rand();Regs(regs);regs[15]=0x8c000000;
		memcpy(m_r,regs,sizeof(regs));m_sr=sr;m_ea=ea;
		const unsigned miss=c%65,limit=(c&1)?12:6;
		Sh3Native::Emitter legacy(old);
		for(unsigned i=0;i<64;i++){
			UINT16 op;
			if(i==miss)op=0x60f2;
			else do {op=UINT16(Rand());}while(!Sh3V2::Candidate(op)||Sh3V2::MemoryOp(op)||
				((op>>8)&15)>=limit||(((op>>12)>=1&&(op>>12)<=6&&(op>>12)!=4)&&((op>>4)&15)>=limit));
			ops[i]=op;
			Check(legacy.Emit(op,i),"legacy mixed emit");
			if(i<miss)execute_one(op);
		}
		legacy.Finish(64);
		Sh3V2::CompactEmitter e(words);
		Check(e.Build(ops,64),"mixed fixed bindings");
		wide+=e.Wide();
		oldBytes+=(legacy.End()-old)*4;newBytes+=(e.End()-words)*4;
		Check(cache.Publish(words,e.End()-words,offset),"mixed publish");
		const Sh3Native::Memory maps={MemMapR,MemMapRL,MemMapW,NULL,0,0,0};
		MemMapRL[0xc00]=(UINT8*)1;
		register UINT64 keep19 asm("x19")=0x123456789abcdef0ull;
		register UINT64 keep28 asm("x28")=0xfedcba9876543210ull;
		asm volatile("" : "+r"(keep19),"+r"(keep28));
		unsigned done=cache.RunCompact(offset,regs,&sr,&ea,maps,e.Wide());
		asm volatile("" : "+r"(keep19),"+r"(keep28));
		Check(keep19==0x123456789abcdef0ull && keep28==0xfedcba9876543210ull,"callee-saved ABI");
		CheckState(regs,sr,ea,done,miss,c,"mixed guard position");
		MemMapRL[0xc00]=(UINT8*)data;
	}
	Check(wide>0 && wide<4096,"wide and leaf ABI exercised");
	printf("V2 compact mixed blocks/guard positions: 4096 (%u wide) PASS\n",wide);
	printf("V2 mixed payload bytes: legacy %llu compact %llu\n",(unsigned long long)oldBytes,(unsigned long long)newBytes);
}
static void Stores(){
	Setup(10,0);
	Sh3V2::CodeCache cache;
	const UINT16 stores[]={0x2f00,0x2f01,0x2f02,0x2f04,0x2f05,0x2f06,0x1f03};
	unsigned tested=0;
	for(unsigned wide=0;wide<2;wide++)for(UINT16 op:stores){
		UINT16 ops[17];for(unsigned i=0;i<16;i++)ops[i]=0x7001|((i%(wide?10:5))<<8);ops[16]=op;
		UINT32 words[Sh3Native::MaxWords],offset;
		Sh3V2::CompactEmitter e(words);Check(e.Build(ops,17),"store prefix build");
		Check(cache.Publish(words,e.End()-words,offset),"store prefix publish");
		const unsigned bytes=(op>>12)==1?4:1u<<(op&3);
		const bool pre=(op>>12)==2&&(op&15)>=4;
		for(unsigned dest=90;dest<240;dest++){
			for(unsigned i=0;i<128;i++)data[i]=UINT16(i*173+dest);
			UINT16 before[128],expected[128];memcpy(before,data,sizeof(before));
			UINT32 regs[16],sr=Rand(),ea=Rand();Regs(regs);
			regs[15]=0xac000000+dest+(pre?bytes:0)-((op>>12)==1?12:0);
			memcpy(m_r,regs,sizeof(regs));m_sr=sr;m_ea=ea;
			for(unsigned i=0;i<16;i++)execute_one(ops[i]);
			const unsigned host=bytes==1?dest^1:dest;
			const bool guard=(dest&(bytes-1))||(host<230&&host+bytes>102);
			if(!guard)execute_one(op);
			memcpy(expected,data,sizeof(expected));memcpy(data,before,sizeof(before));
			const Sh3Native::Memory maps={MemMapR,MemMapRL,MemMapW,(UINT8*)data+102,128,0,0};
			const unsigned done=cache.RunCompact(offset,regs,&sr,&ea,maps,e.Wide());
			CheckState(regs,sr,ea,done,guard?16:17,op,"store overlap");
			Check(!memcmp(expected,data,sizeof(expected)),"alias overlap memory");
			++tested;
		}
	}
	printf("V2 compact store alias/overlap/alignment and prefix state: %u PASS\n",tested);
	// Representative memory-heavy 32-op span, repeated guards.
	UINT16 ops[32];for(unsigned i=0;i<32;i++)ops[i]=(i&1)?0x2132:0x6312;
	UINT32 a[Sh3Native::MaxWords],b[Sh3Native::MaxWords];
	Sh3Native::Emitter old(a);for(unsigned i=0;i<32;i++)Check(old.Emit(ops[i],i),"legacy size");old.Finish(32);
	Sh3V2::CompactEmitter e(b);Check(e.Build(ops,32),"compact size");
	printf("V2 memory-heavy 32-op payload: %u -> %u bytes\n",unsigned((old.End()-a)*4),unsigned((e.End()-b)*4));
}
int main(){
	setvbuf(stdout,NULL,_IOLBF,0);
	Encodings();Mixed();Stores();return 0;
}
