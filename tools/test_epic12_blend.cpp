// Actual EPIC12 driver differential test, using the original scalar path.
#include "../src/burn/devices/epic12.cpp"
#if !EPIC12_NEON
#error AArch64 NEON required
#endif
#ifdef fprintf
#undef fprintf
#endif
#include <vector>
static INT32 TestLog(INT32,TCHAR*,...){return 0;}
INT32 (__cdecl *bprintf)(INT32,TCHAR*,...)=TestLog;
static std::vector<UINT8> stateStream;
static size_t statePos=0;
static bool stateLoading=false;
static INT32 __cdecl TestScan(BurnArea *area){
	if(stateLoading){
		if(statePos+area->nLen>stateStream.size())return 1;
		memcpy(area->Data,stateStream.data()+statePos,area->nLen);
		statePos+=area->nLen;
	} else {
		const UINT8 *p=(const UINT8*)area->Data;
		stateStream.insert(stateStream.end(),p,p+area->nLen);
	}
	return 0;
}
INT32 (__cdecl *BurnAcb)(BurnArea *)=TestScan;
static uint32_t rng=0x919f55u;
static uint32_t Random(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static uint32_t original[0x2000*3], scalar[0x2000*3], vectorized[0x2000*3];
static UINT16 packed[0x2000*3];
static void Tables(){
	for(unsigned y=0;y<64;y++)for(unsigned x=0;x<32;x++){
		unsigned v=x*y/31;
		epic12_device_colrtable[x][y]=epic12_device_colrtable_rev[x^31][y]=v>31?31:v;
	}
	for(unsigned y=0;y<32;y++)for(unsigned x=0;x<32;x++)epic12_device_colrtable_add[x][y]=x+y>31?31:x+y;
}
#if THREADY == THREADY_PTHREAD
static bool ParallelTest(){
	const size_t pixels=0x2000*128;
	std::vector<uint32_t> base(pixels),serial(pixels),parallel(pixels);
	for(size_t i=0;i<pixels;i++) base[i]=Epic12Fast::Expand(uint16_t(Random()));
	UINT16 commands[4096]={};
	m_ram16_copy=commands;
	m_main_rammask=sizeof(commands)-1;
	m_main_ramsize=sizeof(commands);
	m_gfx_addr_shadowcopy=0;
	m_gfx_clip_x_shadowcopy=64;
	m_gfx_clip_y_shadowcopy=16;
	m_clip_margin_shadowcopy=0;
	blitHelper.init(gfx_helper_cb);
	if(!blitHelper.is_threaded())return false;
	bool pass=true;
	for(unsigned c=0;c<24&&pass;c++){
		unsigned n=0;
		auto put=[&](UINT16 value){commands[n++]=value;};
		for(unsigned k=0;k<20;k++){
			if(k%5==0){put(0xc000);put((k/5+c)&1);}
			const UINT16 mode=(k%4==0?0:0x200)|((k+c)&7)|(((k*3+c)&7)<<4)
				|((k&1)?0x100:0)|((k&2)?0x400:0)|((k&4)?0x800:0);
			put(0x1000|mode);put(Random());put(1024+(k%3)*384);put(0);
			put(48+(k%4)*16);put((k%3)*8);
			put(319);put(95);put(k%2?0x70:0x80);put(k%2?0x9080:0x8080);
		}
		put(0);
		serial=base;parallel=base;
		m_bitmaps=serial.data();epic12_device_blit_delay=0;gfx_exec();
		const UINT64 delay=epic12_device_blit_delay;
		m_bitmaps=parallel.data();epic12_device_blit_delay=0;
		if(!gfx_exec_parallel()||delay!=epic12_device_blit_delay||serial!=parallel){
			fprintf(stderr,"EPIC12 parallel mismatch/rejection case=%u\n",c);pass=false;
		}
		int split=0;
		// Source/destination dependency (first sprite now reads the output area).
		const UINT16 saved=commands[4];commands[4]=64;
		if(gfx_split_plan(split))pass=false;
		commands[4]=saved;
		// Source wrapping / uploads must never be reordered.
		commands[4]=8190;if(gfx_split_plan(split))pass=false;commands[4]=saved;
		const UINT16 attr=commands[2];commands[2]=0x2000;
		if(gfx_split_plan(split))pass=false;
		commands[2]=attr;
	}
	// The unclipped source rectangle overlaps the framebuffer, but its
	// actual sampled pixels do not. Test both X and Y flip origins and
	// negative destination clipping against the serial device.
	for (unsigned c=0;c<12&&pass;c++) {
		unsigned n=0;
		auto put=[&](UINT16 value){commands[n++]=value;};
		put(0xc000);put(1);
		for(unsigned k=0;k<12;k++){
			const bool flip=c&1;
			const UINT16 attr=0x1000|0x100|((k&1)?0x200:0)|((c&2)?0x400:0)|(flip?0x800:0);
			put(attr);put(0x7880);put(64);put(0);
			put(flip?64:UINT16(-256));put(16);
			put(639);put(79);put(0x80);put(0x8080);
		}
		put(0);
		serial=base;parallel=base;
		m_bitmaps=serial.data();epic12_device_blit_delay=0;gfx_exec();
		const UINT64 delay=epic12_device_blit_delay;
		m_bitmaps=parallel.data();epic12_device_blit_delay=0;
		if(!gfx_exec_parallel()||delay!=epic12_device_blit_delay||serial!=parallel){
			fprintf(stderr,"EPIC12 clipped read footprint mismatch case=%u\n",c);pass=false;
		}
	}
	blitHelper.exit();
	if(pass)printf("EPIC12 real dual-worker command lists: 36 cases PASS; clipped/flipped footprint and dependency/wrap/upload fallback PASS\n");
	return pass;
}

static bool SegmentedTest(){
	const size_t pixels=0x2000*128;
	std::vector<UINT32> base(pixels),serial(pixels),parallel(pixels);
	std::vector<UINT16> commands(1<<17),shadow(commands.size());
	for (auto &p:base) p=Epic12Fast::Expand(UINT16(Random()));
	m_main_ramsize=commands.size()*2;
	m_main_rammask=m_main_ramsize-1;
	m_gfx_clip_x_shadowcopy=64;
	m_gfx_clip_y_shadowcopy=16;
	m_clip_margin_shadowcopy=0;
	EP1C_CLIP_MARGIN=0;
	m_ram16=commands.data();
	m_ram16_copy=shadow.data();
	blitHelper.init(gfx_helper_cb);
	if (!blitHelper.is_threaded()) return false;
	bool pass=true;
	unsigned n=0;
	auto put=[&](UINT16 v) {commands[n++ & (commands.size()-1)]=v;};
	auto clip=[&](bool on) {put(0xc000);put(on);};
	auto draw=[&](unsigned mode,int sx,int sy,int dx,int dy,int w,int h) {
		put(0x1000|mode);put(0x9870);put(sx);put(sy);put(dx);put(dy);
		put(w-1);put(h-1);put(0x74);put(0x9080);
	};
	auto run=[&](unsigned c,int sx,int dx) {
		for (unsigned k=0;k<8;k++)
			draw(((c+k)&3)*0x400|0x300|((c+k)&7)|(((c+k*3)&7)<<4),sx,16,dx,16,320,96);
	};
	auto upload=[&](int x,int y,int w,int h) {
		put(0x2000);put(0);put(0x9999);put(0x9999);put(x);put(y);put(w-1);put(h-1);
		for (int i=0;i<w*h;i++) put(Random()); // includes command-like words
	};
	for (unsigned c=0;c<32 && pass;c++) {
		std::fill(commands.begin(),commands.end(),0);
		// Also exercise command/header/payload reads across shadow RAM wrap.
		n=(c&1) ? commands.size()-11 : 0;
		m_gfx_addr=m_gfx_addr_shadowcopy=n*2;
		clip(false);
		upload(1024,16,320,96); // leading upload, later consumed with flips
		run(c,1024,64);
		clip(true);
		upload(128,32,33,17); // overwrites output from the preceding run
		run(c+1,2048,32); // clipped left edge, inherited clip after upload
		clip(false);
		run(c+2,64,1536); // RAW dependency on the preceding framebuffer
		run(c+3,3072,64); // WAR: overwrites source of the preceding run
		// Self alias and offset vertical overlap must stay serial, with flips.
		draw(0x300|((c&3)*0x400),64,16,65,17,319,94);
		draw((c&3)*0x400,8190,16,64,16,32,80); // source X wraps
		clip(true);
		draw(0x900,1024,16,-256,16,640,80); // original flip origin after clipping
		run(c+4,2048,64);
		upload(70,20,5,3); // trailing upload cannot run before the last join
		put((c&2) ? 0xf000 : 0);
		m_blit_delay_ns=0;m_blit_idle_op_bytes=0;
		gfx_create_shadow_copy();
		const UINT64 ns=m_blit_delay_ns;
		const UINT16 idle=m_blit_idle_op_bytes;
		const auto snapshot=shadow;
		// A submitted list uses the snapshot even if guest RAM is changed.
		std::fill(commands.begin(),commands.end(),0xffff);
		serial=base;parallel=base;
		m_bitmaps=serial.data();epic12_device_blit_delay=19;gfx_exec();
		const UINT64 delay=epic12_device_blit_delay;
		Epic12Segment segments[EPIC12_MAX_SEGMENTS];unsigned count=0,splits=0;
		if (!gfx_segment_plan(segments,count)) {pass=false;break;}
		for (unsigned i=0;i<count;i++) splits+=segments[i].split_y!=0;
		if (splits<4) {pass=false;break;}
		m_bitmaps=parallel.data();epic12_device_blit_delay=19;
		if (!gfx_exec_parallel() || serial!=parallel || delay!=epic12_device_blit_delay ||
			m_blit_delay_ns!=ns || m_blit_idle_op_bytes!=idle || shadow!=snapshot || blitHelper.ok_to_wait) {
			fprintf(stderr,"EPIC12 segmented mismatch case=%u ranges=%u splits=%u\n",c,count,splits);pass=false;
		}
		if(c<4 && pass) {
			std::vector<UINT16> compact(pixels);
			for(size_t i=0;i<pixels;i++)compact[i]=Epic12Packed::Pack(base[i]);
			m_bitmaps16=compact.data();m_bitmaps=nullptr;epic12_device_blit_delay=19;
			if(!gfx_exec_parallel() || delay!=epic12_device_blit_delay)pass=false;
			for(size_t i=0;i<pixels && pass;i++)if(Epic12Fast::Expand(compact[i])!=serial[i])pass=false;
			m_bitmaps16=nullptr;
			if(!pass)fprintf(stderr,"EPIC12 packed mixed upload/parallel list mismatch case=%u\n",c);
		}
	}
	// Dependency-only lists must recover too (without an upload trigger).
	for (unsigned c=0;c<8 && pass;c++) {
		n=0;m_gfx_addr_shadowcopy=0;m_ram16_copy=commands.data();
		clip(false);run(c,1024,64);run(c+1,64,1536);run(c+2,3072,64);put(0);
		serial=base;parallel=base;
		m_bitmaps=serial.data();epic12_device_blit_delay=0;gfx_exec();
		const UINT64 delay=epic12_device_blit_delay;
		m_bitmaps=parallel.data();epic12_device_blit_delay=0;
		if (!gfx_exec_parallel() || serial!=parallel || delay!=epic12_device_blit_delay) pass=false;
	}
	// Many consecutive uploads should coalesce into a single serial range,
	// not consume the segment cap or lose their original ordering.
	if (pass) {
		n=0;m_gfx_addr_shadowcopy=0;m_ram16_copy=commands.data();
		clip(false);run(0,1024,64);
		for(unsigned k=0;k<EPIC12_MAX_SEGMENTS+4;k++) upload(1024,16,1,1);
		run(1,1024,64);put(0);
		serial=base;parallel=base;
		m_bitmaps=serial.data();epic12_device_blit_delay=0;gfx_exec();
		const UINT64 delay=epic12_device_blit_delay;
		m_bitmaps=parallel.data();epic12_device_blit_delay=0;
		if (!gfx_exec_parallel() || serial!=parallel || delay!=epic12_device_blit_delay) pass=false;
		// Exercise the actual job callback and diagnostics, including the
		// runtime A/B fallback; a segmented job counts as one split job.
		for (unsigned mode=0;mode<2 && pass;mode++) {
			epic12_set_perf_enabled(true);
			blitSegmentsEnabled=mode!=0;
			blitParallelReady=blitParallelAllowed=blitJobAsync=true;
			parallel=base;m_bitmaps=parallel.data();
			run_blitter_cb();
			Epic12PerfStats stats={};
			epic12_get_perf_stats(&stats);
			if (serial!=parallel || delay!=epic12_device_blit_delay ||
				stats.parallelJobs!=mode || stats.parallelFallback!=1-mode ||
				stats.segmentedJobs!=mode || stats.segmentedRanges!=2*mode ||
				stats.splitReject[EPIC12_SPLIT_UPLOAD]!=1-mode) pass=false;
		}
		epic12_set_perf_enabled(false);
		blitSegmentsEnabled=true;blitParallelReady=blitParallelAllowed=false;
	}
	// A rejected plan must be side-effect free even after a profitable prefix.
	// Unknown tail, segment cap, command cap, truncated upload / cyclic RAM,
	// invalid upload bounds, tiny and unbalanced runs.
	for (unsigned c=0;c<8 && pass;c++) {
		n=0;m_gfx_addr_shadowcopy=0;m_ram16_copy=commands.data();
		std::fill(commands.begin(),commands.end(),0);
		clip(false);
		if (c<6) run(c,1024,64);
		upload(1024,16,1,1);
		if (c==0) put(0x3000);
		if (c==1) for (unsigned k=0;k<EPIC12_MAX_SEGMENTS;k++) {
			// Adjacent serial uploads are coalesced; alternate substantial
			// independent draw runs with uploads to exercise the real cap.
			run(k,1024,64);upload(1024,16,1,1);
		}
		if (c==2) for (unsigned k=0;k<8192;k++) clip(k&1);
		if (c==3) {
			put(0x2000);put(0);put(0);put(0);put(0);put(0);put(8191);put(4095);
		}
		if (c==4) upload(8191,16,2,1);
		if (c==5) { // no terminator within one full RAM traversal
			while (n<commands.size()) put(0xc000);
		}
		if (c==6) draw(0,1024,16,64,16,32,32);
		if (c==7) for (unsigned k=0;k<80;k++) draw(0,1024,16,64,16,819,3);
		if (c!=5) put(0);
		parallel=base;m_bitmaps=parallel.data();epic12_device_blit_delay=123;
		if (gfx_exec_parallel() || parallel!=base || epic12_device_blit_delay!=123 || blitHelper.ok_to_wait) {
			fprintf(stderr,"EPIC12 segmented fallback mutated/replayed case=%u\n",c);pass=false;
		}
		// Safe malformed/small inputs retain the normal serial fallback result.
		if (c==0 || c==2 || c>=6) {
			gfx_exec();
			const UINT64 delay=epic12_device_blit_delay;
			serial=base;m_bitmaps=serial.data();epic12_device_blit_delay=123;gfx_exec();
			if (serial!=parallel || delay!=epic12_device_blit_delay) pass=false;
		}
	}
	blitHelper.exit();
	if (pass) printf("EPIC12 segmented lists: 41 differential cases PASS; 8 bounded/side-effect-free fallbacks and runtime counters PASS\n");
	else fprintf(stderr,"EPIC12 segmented test FAILED\n");
	return pass;
}

// Precise row ownership: conservative tiles overlap, but the exact
// cross-lane read/write rectangles either prove safety or reject it.
static bool PreciseLaneTest(){
	const size_t pixels=0x2000*128;
	std::vector<UINT32> base(pixels),serial(pixels),parallel(pixels);
	std::vector<UINT16> commands(1<<17);
	for (auto &p:base) p=Epic12Fast::Expand(UINT16(Random()));
	m_main_ramsize=commands.size()*2;m_main_rammask=m_main_ramsize-1;
	m_ram16_copy=commands.data();m_gfx_addr_shadowcopy=0;
	m_gfx_clip_x_shadowcopy=64;m_gfx_clip_y_shadowcopy=16;m_clip_margin_shadowcopy=0;
	blitHelper.init(gfx_helper_cb);
	if (!blitHelper.is_threaded()) return false;
	bool pass=true;
	unsigned n=0;
	auto put=[&](UINT16 v){commands[n++]=v;};
	auto draw=[&](unsigned mode,int sx,int sy,int dx,int dy,int w,int h){
		put(0x1000|mode);put(0x9870);put(sx);put(sy);put(dx);put(dy);
		put(w-1);put(h-1);put(0x74);put(0x9080);
	};
	auto reset=[&](){n=0;put(0xc000);put(0);};
	auto upload=[&](){
		put(0x2000);put(0);put(0x9999);put(0x9999);put(0);put(0);put(0);put(0);put(Random());
	};
	unsigned tested=0;
	for (unsigned c=0;c<48 && pass;c++){
		const unsigned scenario=c%4;
		reset();
		if(scenario==3)upload();
		for (unsigned k=0;k<12;k++){
			const unsigned mode=0x300|((c+k)&7)|(((c+3*k)&7)<<4)|((c&4)?0x800:0);
			switch(scenario){
				case 0: // Same coarse X tile, exact rectangles disjoint, Y flip.
					draw(mode|0x400,1,16,321,16,320,96);break;
				case 1: // True RAW/WAR dependencies within one lane only.
					if(k&1)draw(mode,64,16,1024,16,320,96);
					else draw(mode,1024,16,64,16,320,96);
					break;
				case 2: case 3: // Same-row self-overlap, preserve each row's order.
					draw(mode,64,16,65,16,320,96);break;
			}
		}
		put(0);
		blitPreciseEnabled=false;
		int split=0;
		if (gfx_split_plan(split)) {fprintf(stderr,"legacy tile check unexpectedly accepted %u\n",c);pass=false;break;}
		serial=base;m_bitmaps=serial.data();epic12_device_blit_delay=0;gfx_exec();
		const UINT64 delay=epic12_device_blit_delay;
		parallel=base;m_bitmaps=parallel.data();
		blitPreciseEnabled=true;
		epic12_set_perf_enabled(true);
		blitParallelReady=blitParallelAllowed=blitJobAsync=true;
		run_blitter_cb();
		Epic12PerfStats stats={};epic12_get_perf_stats(&stats);
		if (serial!=parallel || delay!=epic12_device_blit_delay ||
			stats.parallelJobs!=1 || stats.laneRanges==0 ||
			stats.segmentedJobs!=(scenario==3?1:0) || blitHelper.ok_to_wait) {
			fprintf(stderr,"precise row ownership mismatch case=%u split=%llu lanes=%llu segments=%llu\n",c,
				(unsigned long long)stats.parallelJobs,(unsigned long long)stats.laneRanges,
				(unsigned long long)stats.segmentedJobs);pass=false;
		}
		epic12_set_perf_enabled(false);blitParallelReady=blitParallelAllowed=false;
		tested++;
	}
	// True cross-lane RAW/WAR, including a single self-overlapping
	// flipped or vertically shifted sprite, cannot run as two bands.
	for (unsigned c=0;c<8 && pass;c++){
		reset();
		for (unsigned k=0;k<12;k++){
			const unsigned mode=0x300|((c&1)?0x400:0)|((c&2)?0x800:0);
			if(c&4) {
				draw(mode,64,16,64,17,320,96);
			} else {
				draw(mode|0x400,64,16,64,16,320,96);
			}
		}
		put(0);
		int split=0;Epic12SplitReject why=EPIC12_SPLIT_OTHER;
		if(gfx_split_plan(split,&why) || why!=EPIC12_SPLIT_DEPENDENCY) {
			fprintf(stderr,"unsafe cross-lane list accepted case=%u\n",c);pass=false;break;
		}
		parallel=base;m_bitmaps=parallel.data();epic12_device_blit_delay=123;
		if(gfx_exec_parallel() || parallel!=base || epic12_device_blit_delay!=123) {
			fprintf(stderr,"unsafe fallback mutated case=%u\n",c);pass=false;
		}
	}
	// A complex list must fall back when exact rectangle storage is
	// exhausted, rather than accepting a partially checked list.
	if(pass){
		reset();
		for(unsigned k=0;k<Epic12ExactSet::Limit+1;k++) draw(0,64,16,65,16,32,96);
		put(0);int split=0;
		if(gfx_split_plan(split))pass=false;
	}
	blitHelper.exit();
	blitPreciseEnabled=true;
	if(pass)printf("EPIC12 exact cross-lane ownership: %u differential lists, 8 true hazards and bounded scratch PASS\n",tested);
	return pass;
}

// Complete self-overlapping flipped draws cannot be sliced horizontally,
// but independent regions can each retain the entire original command.
static bool TileCommandTest(){
	const size_t pixels=0x2000*128;
	std::vector<UINT32> base(pixels),serial(pixels),parallel(pixels);
	std::vector<UINT16> commands(1<<17),compact(pixels);
	for(auto &v:base)v=Epic12Fast::Expand(UINT16(Random()));
	m_main_ramsize=commands.size()*2;m_main_rammask=m_main_ramsize-1;
	m_ram16_copy=commands.data();m_gfx_addr_shadowcopy=0;
	m_gfx_clip_x_shadowcopy=64;m_gfx_clip_y_shadowcopy=16;m_clip_margin_shadowcopy=0;
	blitHelper.init(gfx_helper_cb);
	if(!blitHelper.is_threaded())return false;
	unsigned n=0;
	auto put=[&](UINT16 v){commands[n++]=v;};
	auto clip=[&](bool on){put(0xc000);put(on);};
	auto draw=[&](unsigned mode,int sx,int sy,int dx,int dy,int w,int h){
		put(0x1000|mode);put(0x9870);put(sx);put(sy);put(dx);put(dy);put(w-1);put(h-1);put(0x74);put(0x9080);
	};
	auto upload=[&](unsigned x){
		put(0x2000);put(0);put(0x9999);put(0x9999);put(x);put(16);put(7);put(7);
		for(unsigned k=0;k<64;k++)put(Random());
	};
	auto independent=[&](unsigned mode){
		for(unsigned k=0;k<4;k++){
			draw(mode|0x400,64,16,64,16,320,96);
			draw(mode|0x400,512,16,512,16,320,96);
		}
	};
	bool pass=true;
	for(unsigned c=0;c<32 && pass;c++){
		n=0;clip(false);
		const unsigned mode=0x300|((c&7)<<4)|((c*3)&7)|((c&8)?0x800:0);
		independent(mode);
		if(c&1){
			// One command reads one lane and writes the other, so
			// the scheduler must join before executing that command.
			draw(mode,64,16,512,16,320,96);
			independent(mode);
		}
		if(c&2){
			upload(64);clip(false);independent(mode);
		}
		if(c&4){
			clip(true);draw(mode,512,16,64,16,320,96);
			clip(false);independent(mode);
		}
		put(c&8?0xf000:0);
		int split=0;
		if(gfx_split_plan(split)){fprintf(stderr,"tile fixture unexpectedly row-splittable %u\n",c);pass=false;break;}
		serial=base;m_bitmaps=serial.data();epic12_device_blit_delay=0;gfx_exec();
		const UINT64 delay=epic12_device_blit_delay;
		Epic12TilePlan plan;
		if(!gfx_tile_plan(plan)){fprintf(stderr,"tile plan rejected %u\n",c);pass=false;break;}
		unsigned phases=0;
		for(unsigned i=0;i<plan.phaseCount;i++)phases+=plan.phases[i].parallel;
		if(!phases || ((c&1) && plan.phaseCount<2)){fprintf(stderr,"tile phase count mismatch case=%u phases=%u total=%u\n",c,phases,plan.phaseCount);pass=false;break;}
		parallel=base;m_bitmaps=parallel.data();epic12_set_perf_enabled(true);
		blitParallelReady=blitParallelAllowed=blitTilesEnabled=blitJobAsync=true;
		// For upload lists exercise the new planner explicitly, since
		// the earlier segment fast path may also be valid.
		if(c&2){epic12_device_blit_delay=0;if(!gfx_exec_tiles())pass=false;}
		else run_blitter_cb();
		Epic12PerfStats stats={};epic12_get_perf_stats(&stats);
		if(!pass || serial!=parallel || delay!=epic12_device_blit_delay || stats.tileJobs!=1 ||
			stats.tilePhases!=phases || !stats.tileDraws || blitHelper.ok_to_wait){
			fprintf(stderr,"tile VRAM/delay/counter mismatch case=%u phases=%u actual=%llu\n",c,phases,(unsigned long long)stats.tilePhases);
			pass=false;
		}
		epic12_set_perf_enabled(false);
		if(c<8 && pass){
			for(size_t i=0;i<pixels;i++)compact[i]=Epic12Packed::Pack(base[i]);
			m_bitmaps16=compact.data();m_bitmaps=nullptr;epic12_device_blit_delay=0;
			if(!gfx_exec_tiles() || delay!=epic12_device_blit_delay)pass=false;
			for(size_t i=0;i<pixels&&pass;i++)if(Epic12Fast::Expand(compact[i])!=serial[i])pass=false;
			m_bitmaps16=nullptr;
			if(!pass)fprintf(stderr,"tile packed mismatch case=%u\n",c);
		}
	}
	// A malformed suffix must never cause a partial prefix replay.
	// A same-region list has no independent useful second lane.
	for(unsigned c=0;c<2&&pass;c++){
		n=0;clip(false);
		if(c==0){independent(0x300);put(0x3000);}
		else {for(unsigned k=0;k<12;k++)draw(0x700,64,16,64,16,320,96);put(0);}
		parallel=base;m_bitmaps=parallel.data();epic12_device_blit_delay=123;
		if(gfx_exec_tiles() || parallel!=base || epic12_device_blit_delay!=123)pass=false;
	}
	blitTilesEnabled=blitParallelReady=blitParallelAllowed=false;
	blitHelper.exit();
	if(pass)puts("EPIC12 tile ownership: 32 expanded and 8 packed lists, cross-lane RAW/WAR/WAW barriers and 2 safe fallbacks PASS");
	else puts("EPIC12 tile ownership FAILED");
	return pass;
}

#endif
static bool PackedStateTest(){
	const size_t n=65536+13;
	std::vector<UINT16> source(n),loaded(n);
	std::vector<UINT32> legacy(n);
	for(size_t i=0;i<n;i++){
		source[i]=UINT16(i);legacy[i]=Epic12Fast::Expand(source[i]);
		if(Epic12Packed::Pack(legacy[i])!=source[i])return false;
	}
	m_gfx_size=n;m_bitmaps16=source.data();m_bitmaps=nullptr;
	stateStream.clear();stateLoading=false;
	epic12_scan_vram(ACB_READ);
	if(stateStream.size()!=n*4 || memcmp(stateStream.data(),legacy.data(),n*4))return false;
	m_bitmaps16=loaded.data();statePos=0;stateLoading=true;
	epic12_scan_vram(ACB_WRITE);
	m_bitmaps16=nullptr;stateLoading=false;
	if(source!=loaded || statePos!=n*4)return false;
	puts("EPIC12 packed state: all RGB555 words, byte-identical legacy stream and chunked restore PASS");
	return true;
}
int main(){
	Tables();
	epic12_device_blitfunction *funcs[]={
		epic12_device_f0_ti0_tr0_blit_funcs,epic12_device_f0_ti0_tr1_blit_funcs,
		epic12_device_f0_ti1_tr0_blit_funcs,epic12_device_f0_ti1_tr1_blit_funcs,
		epic12_device_f1_ti0_tr0_blit_funcs,epic12_device_f1_ti0_tr1_blit_funcs,
		epic12_device_f1_ti1_tr0_blit_funcs,epic12_device_f1_ti1_tr1_blit_funcs};
	unsigned cases=0;
	rectangle clip;clip.set(0,0x1fff,0,2);
	for(unsigned group=0;group<8;group++)for(unsigned mode=0;mode<64;mode++)for(unsigned c=0;c<16;c++){
		for(unsigned i=0;i<sizeof(original)/sizeof(original[0]);i++)original[i]=Epic12Fast::Expand(uint16_t(c%4==1 ? Random()&0x7fff : c%4==2 ? Random()|0x8000 : Random()));
		memcpy(scalar,original,sizeof(scalar));memcpy(vectorized,original,sizeof(vectorized));
		// Non-overlapping rows, horizontal overlap and exact alias.
		static const int overlapOffsets[]={-9,-4,-1,0,1,3,4,7};
		const bool overlap=c>=8 || c%4==0;
		int width=c>=8?31+Random()%99:1+Random()%129,srcx=128+Random()%32;
		int dstx=c>=8?srcx+overlapOffsets[c-8]:overlap?srcx+(int(c%7)-3):512;
		int srcy=1,dsty=overlap?1:0;
		int flipy=0,height=1;
		clr_t tint={uint8_t(Random()%64),uint8_t(Random()%64),uint8_t(Random()%64),0};
		uint8_t sa=Random()%32,da=Random()%32;
		epic12_device_blit_delay=0;epic12_blend_neon=false;m_bitmaps=scalar;
		funcs[group][mode](&clip,scalar,srcx,srcy,dstx,dsty,width,height,flipy,sa,da,&tint,&epic12_device_blit_delay);
		UINT64 delay=epic12_device_blit_delay;
		epic12_device_blit_delay=0;epic12_blend_neon=true;m_bitmaps=vectorized;
		funcs[group][mode](&clip,vectorized,srcx,srcy,dstx,dsty,width,height,flipy,sa,da,&tint,&epic12_device_blit_delay);
		if(memcmp(scalar,vectorized,sizeof(scalar)) || delay!=epic12_device_blit_delay){
			fprintf(stderr,"EPIC12 blend mismatch group=%u mode=%u case=%u width=%d\n",group,mode,c,width);return 1;
		}
		// Same real generated rasterizer with the packed backend, scalar
		// and NEON; compare every VRAM pixel and delay to the legacy path.
		for(unsigned simd=0;simd<2;simd++){
			for(unsigned i=0;i<sizeof(packed)/sizeof(packed[0]);i++)packed[i]=Epic12Packed::Pack(original[i]);
			m_bitmaps16=packed;m_bitmaps=nullptr;epic12_blend_neon=simd;
			epic12_device_blit_delay=0;
			funcs[group][mode](&clip,nullptr,srcx,srcy,dstx,dsty,width,height,flipy,sa,da,&tint,&epic12_device_blit_delay);
			for(unsigned i=0;i<sizeof(packed)/sizeof(packed[0]);i++){
				if(Epic12Fast::Expand(packed[i])!=scalar[i]){
					fprintf(stderr,"EPIC12 packed mismatch group=%u mode=%u case=%u simd=%u pixel=%u\n",group,mode,c,simd,i);return 5;
				}
			}
			if(delay!=epic12_device_blit_delay)return 6;
			m_bitmaps16=nullptr;
		}
		cases++;
	}
	printf("EPIC12 packed scalar/NEON: %u cases each PASS (512 modes)\n",cases);
	printf("EPIC12 real scalar/NEON blitter: %u cases PASS (all 512 blend variants)\n",cases);
#if THREADY == THREADY_PTHREAD
	blitPreciseEnabled=false; // Preserve legacy acceptance/rejection fixtures.
	if(!ParallelTest())return 2;
	if(!SegmentedTest())return 3;
	blitPreciseEnabled=true;
	if(!PreciseLaneTest())return 4;
	if(!TileCommandTest())return 8;
	if(!PackedStateTest())return 7;
#endif
}
