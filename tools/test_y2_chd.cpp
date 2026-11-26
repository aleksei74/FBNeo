// Exercise real libchdr + BurnHDDReadBytes with a synthetic compressed
// fixture. Local stdio adapters stand in for the frontend VFS.
#include "../src/burn/burn_hdd.cpp"
#include <thread>
#include <stdio.h>
#undef FILE
#undef fopen
#undef fclose
#undef fread
#undef fwrite
#undef ftell
#undef fseek
#undef fprintf
extern "C" RFILE* rfopen(const char* p,const char* m){return reinterpret_cast<RFILE*>(::fopen(p,m));}
extern "C" int rfclose(RFILE* s){return ::fclose(reinterpret_cast<FILE*>(s));}
extern "C" int64_t rftell(RFILE* s){return ::ftello(reinterpret_cast<FILE*>(s));}
extern "C" int64_t rfseek(RFILE* s,int64_t p,int o){return ::fseeko(reinterpret_cast<FILE*>(s),p,o);}
extern "C" int64_t rfread(void* d,size_t z,size_t n,RFILE* s){return ::fread(d,z,n,reinterpret_cast<FILE*>(s));}
extern "C" int64_t rfwrite(const void* d,size_t z,size_t n,RFILE* s){return ::fwrite(d,z,n,reinterpret_cast<FILE*>(s));}
#ifndef bprintf
static INT32 __cdecl NoLog(INT32,TCHAR*,...){return 0;}
INT32 (__cdecl *bprintf)(INT32,TCHAR*,...)=NoLog;
#endif
static void Require(bool x,const char* s){if(!x){fprintf(stderr,"FAIL: %s\n",s);exit(1);}}
static BurnHDD* Open(const char* path,bool ahead){
	BurnHDD *d=new BurnHDD;
	d->pFile=rfopen(path,"rb");d->pChd=NULL;d->pCache=NULL;d->bDiffDirty=false;
	d->nCylinders=d->nHeads=d->nSectors=0;
	strcpy(d->szImagePath,path);
	Require(d->pFile && !OpenChd(d),"open synthetic CHD");
	Require(d->nHunkBytes==16896 && d->nSectorSize==4224,"fixture geometry");
	Require(BurnHDDConfigureReadAhead(d,ahead)==ahead,"enable read-ahead");
	return d;
}
static UINT8 Expected(UINT64 off){
	const UINT32 i=off%16896,h=off/16896;
	return UINT8(h*31+i*17+(i>>8));
}
static void Compare(BurnHDD *a,BurnHDD *b,UINT64 off,UINT32 len){
	std::vector<UINT8>x(len),y(len);
	INT32 ra=BurnHDDReadBytes(a,off,len,x.data()),rb=BurnHDDReadBytes(b,off,len,y.data());
	Require(ra==rb && x==y,"demand vs speculative bytes");
	UINT64 size=BurnHDDGetSize(a);
	for(UINT32 i=0;i<len;i++)Require(x[i]==(off+i<size?Expected(off+i):0),"fixture byte mismatch");
}
int main(int argc,char**argv){
	Require(argc==2,"fixture filename required");
	BurnHDD *a=Open(argv[1],false),*b=Open(argv[1],true);
	// Disable the new direct range path on the reference side.
	a->bReadStats=false;
	for(unsigned h=0;h<96;h++){
		Compare(a,b,UINT64(h)*16896,4224);
		Compare(a,b,UINT64(h)*16896+4224,4224);
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	UINT32 rng=0x123fbcdu;
	for(unsigned i=0;i<1000;i++){
		rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;
		Compare(a,b,rng%(96*16896),1+(rng%40000));
	}
	Compare(a,b,96*16896-13,64);
	Compare(a,b,96*16896+7,64);
	BurnHDDReadStats st;BurnHDDGetReadStats(b,&st);
	Require(st.readAheadIssued && st.readAheadHits && !st.readAheadFailed,"actual CHD read-ahead unused or failed");
	printf("Real compressed CHD: demand %llu issued %llu hits %llu waited %llu; 1194 range comparisons PASS\n",
		(unsigned long long)st.demandHunks,(unsigned long long)st.readAheadIssued,
		(unsigned long long)st.readAheadHits,(unsigned long long)st.readAheadWaited);
	// Overlay semantics, including partial sectors, are preserved.
	UINT8 write[4224];memset(write,0xc3,sizeof(write));
	Require(!BurnHDDWrite(a,3,1,write) && !BurnHDDWrite(b,3,1,write),"diff write");
	UINT8 x[10000],y[10000];
	Require(!BurnHDDReadBytes(a,3*4224-7,sizeof(x),x) && !BurnHDDReadBytes(b,3*4224-7,sizeof(y),y) && !memcmp(x,y,sizeof(x)),"diff overlay mismatch");
	for(unsigned i=7;i<7+4224;i++)Require(y[i]==0xc3,"diff bytes");
	a->bDiffDirty=b->bDiffDirty=false;
	BurnHDDClose(a);BurnHDDClose(b);
	puts("CHD diff overlay and clean worker shutdown PASS");
}
