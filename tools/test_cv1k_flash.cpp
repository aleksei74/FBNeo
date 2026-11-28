// Compare CV1000 contiguous serial-flash DMA reads against the original
// byte callback, including word swapping and the saturating page index.
#include "../src/cpu/sh4/sh4.cpp"
#include "../src/burn/devices/serflash.cpp"
#ifdef fprintf
#undef fprintf
#endif
#include <stdio.h>
#include <stdlib.h>
#include <vector>
static INT32 __cdecl NoScan(BurnArea *) { return 0; }
INT32 (__cdecl *BurnAcb)(BurnArea *) = NoScan;
void CpuCheatRegister(INT32, cpu_core_config *) {}
#ifndef bprintf
static INT32 __cdecl NoLog(INT32, TCHAR *, ...) { return 0; }
INT32 (__cdecl *bprintf)(INT32, TCHAR *, ...) = NoLog;
#endif
static UINT32 rng=0x4bde0c19;
static UINT32 Rand(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static UINT8 *handlerRam;
static void __fastcall TestWrite(UINT32 a, UINT8 d) {
#ifdef LSB_FIRST
	handlerRam[(a^1)&0xffff]=d;
#else
	handlerRam[a&0xffff]=d;
#endif
}
#if SH3_NATIVE_V2
static bool TestNativeCodeDMA(){
	// The same host backing page appears in the two Type B/C RAM
	// mirrors. Exercise the real bulk and scalar NAND DMAC paths.
	for(unsigned scalar=0;scalar<2;scalar++){
		Cv1kV2Reset();Sh3MapInit();
		setenv("FBNEO_CV1K_V2_WRITE_AUDIT","1",1);
		if(!Cv1kV2Enable())return false;
		std::vector<UINT16> ram(32768,0x7001);
		std::vector<UINT8> flash(2112,0xe7);
		flash[1]=0x7f;
		Sh3MapMemory((UINT8*)ram.data(),0xc000000,0xc00ffff,MAP_RAM);
		Sh3MapMemory((UINT8*)ram.data(),0xc800000,0xc80ffff,MAP_RAM);
		m_cpu_type=CPU_TYPE_SH3;m_pc=0x8c000100;
		m_delay=m_test_irq=m_cpu_off=0;sh3_end_run=0;m_sh4_icount=1000;
		for(unsigned i=0;i<Cv1kV2Hot;i++)Cv1kV2Observe(m_pc);
		if(!Cv1kV2Find(m_pc) || !cv1kV2WriteWatch[0xc00] ||
			cv1kV2WriteWatch[0xc00]!=cv1kV2WriteWatch[0xc80])return false;
		const auto epoch=cv1kV2Generations[cv1kV2WriteWatch[0xc00]];
		m_flash_page_data=flash.data();m_flash_page_size=flash.size();
		m_flash_enab=1;m_flash_state=READ;m_flash_page_addr=0;m_flash_read_seq=0;
		UINT32 sar=0x10000000,dar=0x0c800100,count=scalar?1:2;
		if(!sh4_dma_transfer(0,0,scalar?0:(1<<14),&sar,&dar,&count) || count ||
			cv1kV2Generations[cv1kV2WriteWatch[0xc00]]==epoch || ram[0x80]==0x7001)return false;
		// A dirty block must not execute the old code. Its immutable
		// cached bytes remain valid only for already completed calls.
		if(Cv1kV2Try(m_pc) || Cv1kV2Find(m_pc) || cv1kV2Stats.changed!=1 ||
			cv1kV2Stats.untracked || !cv1kV2Stats.dirtyChecks)return false;
		Cv1kV2Reset();Sh3MapInit();
	}
	puts("B3 real NAND DMA: bulk/scalar mirrored code invalidation PASS");
	return true;
}
#endif
static bool TestDMA(){
	std::vector<UINT8> page(2112),a(0x20000),b(a.size());
	for(auto &v:page)v=Rand();
	m_flash_page_data=page.data();m_flash_page_size=page.size();
	Sh3MapInit();m_cpu_type=CPU_TYPE_SH3;
	const UINT32 base=0xc000000;
	Sh3SetWriteByteHandler(1,TestWrite);
	for(unsigned c=0;c<240;c++){
		const unsigned incd=c%3,incs=(c/3)%3;
		const bool handler=(c%5)==0;
		const unsigned n=1+Rand()%4100;
		// Increment/decrement across a page boundary, both byte lanes;
		// keep fixed-address transfers and handler pages in the fixtures.
		const UINT32 start=base+0x10000+(c%7)-3;
		const unsigned pos=Rand()%(page.size()+32),seq=Rand()%4;
		m_flash_enab=c%7!=0;m_flash_state=c%4?UINT8(READ):UINT8((c/4)%6);
		a.assign(a.size(),0x91);b=a;
		m_flash_page_addr=pos;m_flash_read_seq=seq;
		UINT32 expectedDst=start,expectedSrc=0x10000000;
		for(unsigned i=0;i<n;i++){
			if(incs==2)expectedSrc--;
			if(incd==2)expectedDst--;
			const UINT8 v=serflash_io_read();
#ifdef LSB_FIRST
			a[(expectedDst-base)^1]=v;
#else
			a[expectedDst-base]=v;
#endif
			if(incs==1)expectedSrc++;
			if(incd==1)expectedDst++;
		}
		const int expectPos=m_flash_page_addr;
		const unsigned expectSeq=m_flash_read_seq;
		m_flash_page_addr=pos;m_flash_read_seq=seq;
		Sh3MapMemory(b.data(),base,base+0x1ffff,MAP_RAM);
		if(handler){
			handlerRam=b.data()+0x10000;
			Sh3MapHandler(1,base+0x10000,base+0x1ffff,MAP_WRITE);
		}
		const bool profile=(c&1)!=0;
		Sh3Cv1kDmaPerfEnable(profile);
		UINT32 sar=0x10000000,dar=start,count=n;
		if(!sh4_dma_transfer(0,0,(incd<<14)|(incs<<12),&sar,&dar,&count) ||
			sar!=expectedSrc || dar!=expectedDst || count ||
			a!=b || m_flash_page_addr!=expectPos || m_flash_read_seq!=expectSeq){
			fprintf(stderr,"serial-flash actual DMA mismatch case=%u modes=%u/%u handler=%d\\n",c,incs,incd,handler);
			return false;
		}
		Sh3Cv1kDmaPerfStats stats={};Sh3Cv1kDmaPerfGet(&stats);
		if(stats.calls!=unsigned(profile) || stats.nandCalls!=unsigned(profile) ||
			stats.bytes!=(profile?n:0) || stats.nandBytes!=(profile?n:0) ||
			stats.ns!=stats.nandNs || (!profile && stats.ns)){
			fprintf(stderr,"DMA diagnostics mismatch %u\n",c);return false;
		}
	}
	Sh3Cv1kDmaPerfEnable(false);
	puts("CV1000 DMA diagnostics: 240 transfers alternating enabled/disabled, byte and call counters PASS");
	puts("CV1000 actual SH3 flash DMA: 240 cross-page, handler and address-mode comparisons PASS");
	return true;
}
int main(){
	std::vector<UINT8> page(2112),a(8500),b(8500);
	m_flash_page_data=page.data();m_flash_page_size=page.size();
	for(unsigned c=0;c<20000;c++){
		for(auto &v:page)v=Rand();
		m_flash_enab=(c%7)!=0;
		m_flash_state=c%3==0?UINT8((c/3)%6):UINT8(READ);
		const unsigned seq=Rand()%4;
		const int pos=Rand()%(page.size()+100);
		const unsigned n=(Rand()%4200)*2;
		a.assign(a.size(),0x6d);b=a;
		m_flash_page_addr=pos;m_flash_read_seq=seq;
		for(unsigned i=0;i<n;i++){
#ifdef LSB_FIRST
			a[4+(i^1)]=serflash_io_read();
#else
			a[4+i]=serflash_io_read();
#endif
		}
		const int expect=m_flash_page_addr;
		const unsigned expectSeq=m_flash_read_seq;
		m_flash_page_addr=pos;m_flash_read_seq=seq;
		serflash_io_read_words(b.data()+4,n);
		if(a!=b || m_flash_page_addr!=expect || m_flash_read_seq!=expectSeq){
			fprintf(stderr,"serial-flash block mismatch case=%u state=%u pos=%d count=%u\n",c,m_flash_state,pos,n);
			return 1;
		}
	}
	puts("CV1000 serial-flash DMA block: 20000 byte/state comparisons PASS");
	if(!TestDMA())return 2;
#if SH3_NATIVE_V2
	if(!TestNativeCodeDMA())return 3;
#endif
	return 0;
}
