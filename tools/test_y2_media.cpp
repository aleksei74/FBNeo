// Differential checks of actual Y2 MVC execution, dependency batches and
// NAND cache policy. Standalone on ARM64 or x86-64; use section GC.
#include "../src/burn/drv/sega/d_y2.cpp"
#include <stdio.h>
#include <vector>
#include <thread>
#include <algorithm>
#ifdef fprintf
#undef fprintf
#endif
#ifndef bprintf
static INT32 __cdecl NoLog(INT32, TCHAR *, ...) { return 0; }
INT32 (__cdecl *bprintf)(INT32, TCHAR *, ...) = NoLog;
#endif

static void Require(bool ok,const char *why) {
	if(!ok){fprintf(stderr,"FAIL: %s\n",why);exit(1);}
}
static uint32_t rng=0x99417ea3u;
static uint32_t Random(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}

static void TestNand(){
	using namespace Y2Nand;
	for(unsigned ways : {1u,4u}) {
		const unsigned count=64,sets=count/ways;
		std::vector<Line> lines(count);
		Cache cache;cache.Init(lines.data(),count,ways);
		unsigned fills=0,evictions=0;
		auto fill=[&](Line *line,unsigned chip,uint32_t record){
			fills++;
			uint32_t tag=(chip<<18)|record;
			for(unsigned i=0;i<4096;i++)line->data[i]=uint8_t((tag*151u+i*19u)^(tag>>(i&15)));
		};
		auto get=[&](unsigned chip,unsigned record){
			bool evicted=false;
			const uint8_t *data=cache.Get(chip,record,fill,evicted);
			evictions+=evicted;
			uint32_t tag=(chip<<18)|(record&0x3ffff);
			for(unsigned i=0;i<4096;i++)Require(data[i]==uint8_t((tag*151u+i*19u)^(tag>>(i&15))),"NAND data/tag mismatch");
		};
		get(0,0);get(0,0x40000);Require(fills==1,"NAND last page or wrap failed");
		get(1,0);Require(fills==2,"NAND chip separation failed");
		cache.Init(lines.data(),count,ways);fills=evictions=0;
		for(unsigned i=0;i<ways;i++)get(0,i*sets);
		Require(fills==ways && !evictions,"NAND first fill");
		get(0,0);Require(fills==ways,"NAND resident page");
		get(0,ways*sets);Require(fills==ways+1 && evictions==1,"NAND LRU eviction");
		if(ways>1){get(0,0);Require(fills==ways+1,"NAND recent page evicted");}
		for(unsigned i=0;i<2000;i++)get(Random()&1,Random()&0x7ffff);
		// Verify rank permutation after repeated replacements.
		for(unsigned s=0;s<sets;s++) {
			unsigned ranks=0;
			for(unsigned w=0;w<ways;w++)ranks|=1u<<lines[s*ways+w].rank;
			Require(ranks==(1u<<ways)-1,"NAND LRU rank corruption");
		}
	}
	puts("Y2 NAND cache: legacy layout, chip/wrap, hits, LRU and random data PASS");
}

static void TestPixelMath(){
	for(unsigned c=0;c<20000;c++){
		uint8_t input[9*17],a[8],b[8];
		int32_t scalar[64],simd[64],other[64],expected[64],res[8];
		for(auto &x:input)x=Random();
		unsigned hx=c&1,hy=(c>>1)&1;
		Y2Media::PredictScalar(input,17,hx,hy,scalar);
		Y2Media::Predict(input,17,hx,hy,simd);
		Require(!memcmp(scalar,simd,sizeof(scalar)),"half-pel prediction");
		for(unsigned i=0;i<64;i++){other[i]=Random()&255;expected[i]=(simd[i]+other[i]+1)/2;}
		Y2Media::Average(simd,other);
		Require(!memcmp(expected,simd,sizeof(expected)),"bidirectional prediction");
		for(auto &x:res)x=int32_t(Random()%262145)-131072;
		Y2Media::StoreScalar(a,res,simd,c&1);
		Y2Media::Store(b,res,simd,c&1);
		Require(!memcmp(a,b,8),"residual clipping");
	}
	puts("Y2 MVC exact prediction, average and saturating output: 20000 PASS");
}

static Y2Job Job(unsigned x,unsigned y,unsigned type,unsigned out=0x10000){
	Y2Job j={};j.type=JOB_MVC;j.width=128;j.height=128;
	j.out=out;j.ref[0]=0x20000;j.ref[1]=0x30000;
	j.cmd[0]=(type<<28)|(x<<16)|(y<<8)|63;
	j.cmd[2]=(Random()&31)|((Random()&31)<<16);
	j.cmd[3]=uint16_t(-int(Random()&31))|(uint32_t(uint16_t(-int(Random()&31)))<<16);
	for(unsigned b=0;b<6;b++){
		j.coef[b][0]=uint16_t(Random()%2048);
		if((x+y+b)%3==0)for(unsigned k=1;k<32;k++)
			if((Random()&7)==0)j.coef[b][k]=(Random()&0xffff)|(Random()<<16);
	}
	return j;
}
static void TestGroups(){
	Y2Media::MvcGroup g;
	Y2Job j=Job(0,0,3);
	Require(g.Append(j,RAM_SIZE),"valid MVC group rejected");
	Require(!g.Append(j,RAM_SIZE),"duplicate macroblock accepted");
	j=Job(1,0,3);j.ref[0]=j.out+32;
	Require(!g.Append(j,RAM_SIZE),"overlapping ref accepted");
	j=Job(1,0,3);j.ref[1]=RAM_SIZE-32;
	Require(!g.Append(j,RAM_SIZE),"out-of-bounds ref accepted");
	j=Job(1,0,2);j.ref[0]=j.out; // unused ref is allowed
	Require(g.Append(j,RAM_SIZE),"unused alias rejected");
	j=Job(2,0,0);j.out+=32;
	Require(!g.Append(j,RAM_SIZE),"mixed layout accepted");
	j=Job(2,0,0);j.width=64;
	Require(!g.Append(j,RAM_SIZE),"mixed width accepted");
	j=Job(8,0,0);
	Require(!g.Append(j,RAM_SIZE),"invalid macroblock accepted");
	j=Job(2,0,4);
	Require(!g.Append(j,RAM_SIZE),"non-MVC type accepted");
	puts("Y2 MVC batch guard: disjoint output/ref, duplicate and invalid layouts PASS");
}

static void TestActualMvc(){
	std::vector<uint8_t> initial(0x40000),serial(initial.size()),parallel(initial.size());
	for(auto &v:initial)v=Random();
	for(unsigned x=0;x<8;x++)for(unsigned u=0;u<8;u++)
		MvcBasis[x][u]=(u?0.5:sqrt(0.125))*cos((2*x+1)*u*3.14159265358979323846/16);
	std::vector<Y2Job> jobs;
	for(unsigned i=0;i<64;i++)jobs.push_back(Job(i%8,i/8,i%4));
	// Duplicate block, reference alias, and distinct output form serial
	// barriers; later groups may consume earlier groups' results.
	jobs.push_back(Job(1,0,0));
	Y2Job alias=Job(0,1,1);alias.ref[0]=alias.out;jobs.push_back(alias);
	for(unsigned i=0;i<64;i++){
		Y2Job j=Job(i%8,i/8,i%4,0x38000);j.ref[0]=0x10000;jobs.push_back(j);
	}
	serial=initial;parallel=initial;
	DrvRAM=serial.data();bY2MediaSimd=false;
	for(const auto &job:jobs)MvcExec(job);
	DrvRAM=parallel.data();bY2MediaSimd=true;
	// Actual driver dispatch uses thready workers and a join between groups.
	bY2MvcBatch=true;bY2Profile=true;
	nY2Workers=3;
	for(int i=0;i<nY2Workers;i++)Y2Workers[i].init(RenderWorkers[i]);
	Y2MediaPerf perf={};RunJobs(jobs.data(),jobs.size(),perf);
	for(int i=0;i<nY2Workers;i++)Y2Workers[i].exit();
	nY2Workers=0;bY2Profile=false;
	Require(perf.mvcJobs==jobs.size() && perf.mvcParallel>=128 && perf.mvcBatches>=2,"actual MVC batching not exercised");
	Require(serial==parallel,"actual serial/parallel MVC output differs");
	printf("Y2 MVC real driver: %zu macroblocks, %llu parallel, all RAM bytes identical\n",jobs.size(),(unsigned long long)perf.mvcParallel);
	DrvRAM=nullptr;
}
// Verify memoization with the actual driver map and write tracking,
// not just a standalone equivalent. Queue generations change when the
// pending map is cleared/swapped; dirty bounds may be smaller with memo.
struct Mark { UINT32 start,bytes;UINT8 level; };
struct MarkResult {
	std::vector<UINT8> pending,tracked,written;
	UINT32 lo,hi;
};
static MarkResult RunMarks(const std::vector<Mark> &marks,bool memo) {
	const unsigned pages=RAM_SIZE>>12;
	std::vector<UINT8> pending(pages),tracked(pages),written(pages);
	PendingMap=pending.data();TrackMap=tracked.data();DrvRamWritten=written.data();
	nCheckpointId=0;nMapMin=nDirtyMin=~0u;nMapMax=nDirtyMax=0;
	memset(WrittenMemo,0,sizeof(WrittenMemo));nWrittenMemo=0;
	FootprintGen=~0u;memset(FootprintMemo,0,sizeof(FootprintMemo));
	nPendingGen=1;bY2FootprintMemo=memo;
	for(unsigned i=0;i<marks.size();i++){
		if(i && i%250==0){
			// Simulate queue reuse after applying/clearing pending protection.
			memset(PendingMap,0,pages);nPendingGen++;
			nMapMin=nDirtyMin=~0u;nMapMax=nDirtyMax=0;
		}
		const auto &m=marks[i];FootprintMark(m.start,m.bytes,m.level);
		// A changed dirty range is consumed by the original ProtectUpdate.
		if(i%13==0){nDirtyMin=~0u;nDirtyMax=0;}
	}
	MarkResult r={pending,tracked,written,nMapMin,nMapMax};
	PendingMap=TrackMap=DrvRamWritten=nullptr;
	return r;
}
static void TestFootprint(){
	std::vector<Mark> marks;
	for(unsigned i=0;i<2000;i++){
		UINT32 start=((Random()%128)*4096)+(Random()%4096);
		UINT32 bytes=1+Random()%16384;
		UINT8 level=(i%3)?PROTECT_READ:PROTECT_WRITE;
		marks.push_back({start,bytes,level});
		marks.push_back({start,bytes,level});
		marks.push_back({start+bytes/4,bytes/2,level});
		marks.push_back({RAM_SIZE-4096,8192,PROTECT_WRITE});
	}
	auto a=RunMarks(marks,false),b=RunMarks(marks,true);
	Require(a.pending==b.pending && a.tracked==b.tracked && a.written==b.written && a.lo==b.lo && a.hi==b.hi,"footprint memo changed page protection or write tracking");
	puts("Y2 footprint memo: 8000 mixed marks, queue generations and tracking PASS");
}
int main(){TestNand();TestPixelMath();TestGroups();TestActualMvc();TestFootprint();}
