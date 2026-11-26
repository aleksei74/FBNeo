#include "../src/burn/burn_hdd_readahead.h"
#include <atomic>
#include <chrono>
#include <thread>
#include <stdio.h>
#include <stdlib.h>
using namespace HddReadAhead;
static void Require(bool b,const char *msg){if(!b){fprintf(stderr,"FAIL: %s\n",msg);exit(1);}}
struct Source{
	std::atomic<unsigned> entered{0};
	std::atomic<bool> hold{false};
};
static bool Read(void *p,uint32_t tag,uint8_t *dest){
	auto &src=*static_cast<Source*>(p);
	src.entered.store(tag+1);
	while(src.hold.load())std::this_thread::yield();
	if(tag==99)return false;
	for(unsigned i=0;i<16896;i++)dest[i]=uint8_t((tag*31u+i*17u)^(i>>8));
	return true;
}
static void Await(Source &s,unsigned tag){
	auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
	while(s.entered.load()!=tag+1 && std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
	Require(s.entered.load()==tag+1,"worker did not start");
}
static void Check(uint8_t *data,unsigned tag){for(unsigned i=0;i<16896;i++)Require(data[i]==uint8_t((tag*31u+i*17u)^(i>>8)),"prefetch data");}
int main(){
	Source s;Worker w;uint8_t data[16896];
	Require(w.Start(sizeof(data),Read,&s),"start");
	for(unsigned i=0;i<128;i++){
		w.Offer(i+100);Await(s,i+100);
		Require(w.Take(i+100,data),"completed or matching in-flight read not used");
		Check(data,i+100);
	}
	s.hold=true;w.Offer(50);Await(s,50);
	Require(!w.Take(51,data),"unrelated demand waited or hit");
	w.Offer(51); // must not replace active buffer
	s.hold=false;Require(w.Take(50,data),"active matching hunk lost");Check(data,50);
	w.Offer(99);Await(s,99);Require(!w.Take(99,data),"failed speculative read returned data");
	auto st=w.GetStats();
	Require(st.hits==129 && st.failed==1,"statistics");
	w.Stop();Require(w.Start(sizeof(data),Read,&s),"restart");w.Offer(7);Await(s,7);w.Stop();
	puts("CHD read-ahead worker: bounded queue, 129 matching reads, unrelated demand, failure, shutdown/restart PASS");
}
