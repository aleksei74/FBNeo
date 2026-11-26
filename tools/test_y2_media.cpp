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
	MvcTransposeBasis();
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
static void TestYuvAndBlend(){
	for(unsigned c=0;c<30000;c++){
		UINT8 y[8],u[4],v[4];UINT32 argb[8];UINT16 packed[8];
		for(auto &x:y)x=Random();
		for(auto &x:u)x=Random();
		for(auto &x:v)x=Random();
		Y2Media::Yuv8(y,u,v,argb);
		for(unsigned i=0;i<8;i++){
			INT32 yy=y[i]-16,uu=u[i/2]-128,vv=v[i/2]-128;
			UINT32 ref=0xff000000u|(Clamp((298*yy+409*vv+128)>>8,0,255)<<16)|
				(Clamp((298*yy-100*uu-208*vv+128)>>8,0,255)<<8)|Clamp((298*yy+516*uu+128)>>8,0,255);
			Require(argb[i]==ref,"YUV exact color mismatch");
		}
		for(unsigned mode=0;mode<3;mode++){
			Y2Media::PackYuv8(argb,packed,mode);
			for(unsigned i=0;i<8;i++)Require(packed[i]==pack_pixel(argb[i],mode),"YUV pixel pack mismatch");
		}
		UINT32 src=Random(),dst=Random();
		for(unsigned sf=0;sf<8;sf++)for(unsigned df=0;df<8;df++)for(unsigned sub=0;sub<2;sub++){
			UINT32 ref=0xff000000u;
			for(unsigned sh=0;sh<24;sh+=8){
				INT32 sv=((src>>sh)&255)*BlendFactor(sf,src,dst,sh),dv=((dst>>sh)&255)*BlendFactor(df,src,dst,sh);
				ref|=UINT32(Clamp(((sub?dv-sv:dv+sv)+127)/255,0,255))<<sh;
			}
			Require(Y2PixelOps::BlendFactors(src,dst,sf,df,sub)==ref,"raster factor blend differs");
		}
	}
	puts("Y2 YUV 30000 x 8 pixels, packing and 3840000 blend cases PASS");
}
static void TestYuvActual(){
	for(unsigned n=0;n<64;n++){
		TextureMorton[n]=0;
		for(unsigned b=0;b<6;b++)TextureMorton[n]|=((n>>b)&1)<<(2*b);
	}
	std::vector<UINT8> initial(0x600000),scalar(initial.size()),simd(initial.size());
	for(auto &v:initial)v=Random();
	const UINT32 shapes[][2]={{16,16},{48,80},{64,64},{80,112},{112,48},{128,128},{640,480},{720,480}};
	unsigned cases=0;
	nY2Workers=2;
	for(int i=0;i<nY2Workers;i++)Y2Workers[i].init(RenderWorkers[i]);
	for(const auto &wh:shapes)for(unsigned mode=0;mode<4;mode++)for(unsigned tiled=0;tiled<2;tiled++){
		Y2Job j={};j.type=JOB_YUV;j.width=wh[0];j.height=wh[1];
		j.control=0x8100|(mode<<12)|(tiled?0x4000:0);
		j.out=0x300000;j.planes[0]=0x10000;j.planes[1]=0x100000;j.planes[2]=0x200000;
		Require(YuvCanDirect(j),"eligible YUV rejected");
		if(tiled||mode==3){
			EdgeTileRanksPrepare(j.width,j.height);
			for(UINT32 y=0;y<j.height;y++)for(UINT32 x=0;x<j.width;x+=2){
				UINT32 a=texture_offset(x,y,j.width,j.height);
				Require(!(a&1) && texture_offset(x+1,y,j.width,j.height)==a+1,"Morton pair not contiguous");
			}
		}
		scalar=initial;simd=initial;
		DrvRAM=scalar.data();bY2YuvSimd=false;YuvExec(j);
		DrvRAM=simd.data();bY2YuvSimd=true;bY2MediaSimd=true;YuvExec(j);
		Require(scalar==simd,"actual YUV conversion/packing differs");
		j.planes[0]=j.out;Require(!YuvCanDirect(j),"aliased YUV accepted");
		j.planes[0]=RAM_SIZE-8;Require(!YuvCanDirect(j),"out of range YUV accepted");
		cases++;
	}
	for(int i=0;i<nY2Workers;i++)Y2Workers[i].exit();
	nY2Workers=0;DrvRAM=nullptr;EdgeTileRanksExit();
	printf("Y2 real YUV: %u layouts including edge tiles and all modes identical\n",cases);
}
static void TestIdct(){
	for(unsigned x=0;x<8;x++)for(unsigned u=0;u<8;u++)
		MvcBasis[x][u]=(u?0.5:sqrt(0.125))*cos((2*x+1)*u*3.14159265358979323846/16);
	MvcTransposeBasis();
	unsigned vectorized=0,ties=0;
	for(unsigned i=0;i<30000;i++){
		INT32 coef[64]={},a[64],b[64];
		for(unsigned j=0;j<64;j++)if(i%4==0 || (Random()%(1+(i%16)))==0)coef[j]=INT16(Random());
		if(i%5==0){memset(coef,0,sizeof(coef));coef[i%64]=INT16(Random());}
		MvcIdctScalar(coef,a);
		if(MvcIdctNeon(coef,b))vectorized++;else{ties++;MvcIdctScalar(coef,b);}
		Require(!memcmp(a,b,sizeof(a)),"IDCT residual differs");
	}
#if Y2_PIXEL_NEON
	Require(vectorized>20000 && ties>0,"IDCT SIMD or tie fallback not exercised");
#endif
	printf("Y2 IDCT: 30000 sparse/dense blocks, %u SIMD, %u fallback, residuals identical\n",vectorized,ties);
}
static void TestReadAheadPolicy(){
	unsetenv("FBNEO_Y2_CHD_READAHEAD");Y2PerfBegin();
	Require(!bY2ReadAhead,"CHD read-ahead default not disabled");
	setenv("FBNEO_Y2_CHD_READAHEAD","0",1);Y2PerfBegin();
	Require(!bY2ReadAhead,"CHD read-ahead explicit off");
	setenv("FBNEO_Y2_CHD_READAHEAD","1",1);Y2PerfBegin();
	Require(bY2ReadAhead,"CHD read-ahead opt-in");
	unsetenv("FBNEO_Y2_CHD_READAHEAD");Y2PerfBegin();bY2Profile=false;
	puts("CHD read-ahead default off and explicit opt-in PASS");
}
int main(){TestReadAheadPolicy();TestNand();TestPixelMath();TestIdct();TestGroups();TestActualMvc();TestFootprint();TestYuvAndBlend();TestYuvActual();}
