// Synthetic AMM packets, no copyrighted sample ROM required.
#include "../src/burn/snd/ymz770.cpp"
#include "../src/burn/snd/mpeg_audio.cpp"
#ifdef fprintf
#undef fprintf
#endif
#include <vector>
#include <stdio.h>
#include <stdlib.h>
INT32 nBurnSoundRate=48000,nBurnSoundLen=800,nBurnFPS=6000,nInterpolation=0;
UINT32 nCurrentFrame=0;
INT16 *pBurnSoundOut=nullptr;
UINT8 *_BurnMalloc(INT32 size,char *,INT32){return (UINT8*)calloc(1,size);}
void _BurnFree(void *p){free(p);}
static INT32 __cdecl NoScan(BurnArea *){return 0;}
INT32 (__cdecl *BurnAcb)(BurnArea *)=NoScan;
#ifndef bprintf
static INT32 __cdecl NoLog(INT32,TCHAR *,...){return 0;}
INT32 (__cdecl *bprintf)(INT32,TCHAR *,...)=NoLog;
#endif
static void Check(bool b,const char *why){if(!b){fprintf(stderr,"FAIL: %s\n",why);exit(1);}}
static INT32 cpuCycles;
static INT32 Cycles(){return cpuCycles;}
static void Write(UINT8 reg,UINT8 value){ymz770_write(0,reg);ymz770_write(1,value);}
static void Start(unsigned ch,unsigned phrase){
	Write(0x40+4*ch,phrase);Write(0x41+4*ch,128);
	Write(0x42+4*ch,8);Write(0x43+4*ch,2);
}
static std::vector<UINT8> Rom(){
	std::vector<UINT8> rom(0x4000+8);
	// Phrase 1 starts with sixteen zero bytes before one valid mono
	// AMM packet. Phrase 2 deliberately has no sync until ROM end.
	rom[5]=0;rom[6]=8;rom[7]=0;
	rom[9]=0;rom[10]=0x20;rom[11]=0;
	unsigned bit=0x810*8;
	auto put=[&](unsigned value,unsigned n){
		for(unsigned k=n;k>0;k--,bit++)
			rom[bit/8]|=((value>>(k-1))&1)<<(7-bit%8);
	};
	put(0xfff,12);put(2,3);put(0,1);
	put(1,4);put(2,2);put(0,2);
	put(3,2);put(0,2);put(2,3);put(0,1);
	// All band allocations zero -> 96 valid silent mono samples.
	return rom;
}
struct Result{
	std::vector<INT16> pcm;
	std::vector<UINT8> channels;
	Ymz770PerfStats cpu,all;
};
static Result Run(std::vector<UINT8> &rom,bool profile){
	Result r={};r.pcm.resize(nBurnSoundLen*2);
	pBurnSoundOut=r.pcm.data();cpuCycles=0;
	ymz770_init(rom.data(),0x4000);
	ymz770_reset();ymz770_set_buffered(Cycles,100000000);
	ymz770_perf_enable(profile);
	Write(0,0);Write(1,128);Write(2,0);
	Start(0,1);
	cpuCycles=200000;Write(1,128);
	Start(1,2);
	cpuCycles=800000;Write(1,128);
	Start(2,1);
	ymz770_perf_get(&r.cpu);
	ymz770_update(pBurnSoundOut,nBurnSoundLen);
	ymz770_perf_get(&r.all);
	r.channels.resize(sizeof(m_channels));
	memcpy(r.channels.data(),m_channels,sizeof(m_channels));
	ymz770_exit();pBurnSoundOut=nullptr;
	return r;
}
int main(){
	auto rom=Rom();
	mpeg_audio a(rom.data(),mpeg_audio::AMM,false,0),b(rom.data(),mpeg_audio::AMM,false,0);
	int pa=0x800*8,pb=pa,na=0,nb=0,ra=0,rb=0,ca=0,cb=0,bits=-1;
	short oa[4096]={},ob[4096]={};
	Check(a.decode_buffer(pa,0x4000*8,oa,na,ra,ca),"valid synthetic packet");
	Check(b.decode_buffer(pb,0x4000*8,ob,nb,rb,cb,&bits),"profiled packet");
	Check(pa==pb && na==96 && na==nb && ra==rb && ca==cb && !memcmp(oa,ob,sizeof(oa)) && bits==16*8+15,"sync search and decoded result");
	pb=0x2000*8;bits=-1;nb=0;
	Check(!b.decode_buffer(pb,0x4000*8,ob,nb,rb,cb,&bits) && pb==0x2000*8 && bits==0x2000*8,"long failed sync scan");
	pb=0x4000*8-8;bits=-1;
	Check(!b.decode_buffer(pb,0x4000*8,ob,nb,rb,cb,&bits) && bits==0,"short input");
	const auto off=Run(rom,false),on=Run(rom,true);
	Check(off.pcm==on.pcm && off.channels==on.channels,"profile changed PCM or channel state");
	Check(off.all.decodes==0 && off.all.writes==0 && off.all.decodeNs==0,"disabled profiler collected data");
	Check(on.cpu.decodes>=2 && on.all.decodes>on.cpu.decodes &&
		on.all.keyOns==3 && on.all.emptyDecodes>=1 && on.all.syncMaxBits==0x2000*8 &&
		on.all.decodedSamples>=192 && on.all.streamSamples>on.cpu.streamSamples &&
		on.all.writeNs==on.cpu.writeNs && on.all.decodeNs>=on.cpu.decodeNs,"buffered write/end render diagnostic attribution");
	printf("CV1000 YMZ770 diagnostics: actual buffered writes, valid and missing AMM sync, PCM/state equality, CPU/end split PASS (decodes %llu/%llu, max scan %llu bits)\n",
		(unsigned long long)on.cpu.decodes,(unsigned long long)on.all.decodes,(unsigned long long)on.all.syncMaxBits);
	return 0;
}
