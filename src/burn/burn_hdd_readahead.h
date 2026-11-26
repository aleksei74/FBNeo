// SPDX-License-Identifier: BSD-3-Clause
// Bounded, read-only speculative hunk decoder. The callback must own a
// separate file/decoder context from demand reads. Only one hunk in flight.
#ifndef BURN_HDD_READAHEAD_H
#define BURN_HDD_READAHEAD_H
#if defined(__linux__)
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
namespace HddReadAhead {
struct Stats { uint64_t issued, hits, waited, discarded, failed; };
class Worker {
	typedef bool (*Reader)(void *,uint32_t,uint8_t *);
	enum State { Empty, Pending, Working, Ready };
	pthread_mutex_t lock_ = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t cond_ = PTHREAD_COND_INITIALIZER;
	pthread_t thread_;
	bool started_=false, stop_=false;
	State state_=Empty;
	uint32_t tag_=0;
	size_t bytes_=0;
	uint8_t *buf_=nullptr;
	Reader read_=nullptr;
	void *ctx_=nullptr;
	Stats stats_={};
	static void *Entry(void *ptr) { static_cast<Worker*>(ptr)->Run(); return nullptr; }
	void Run() {
		pthread_mutex_lock(&lock_);
		while(!stop_) {
			while(!stop_ && state_!=Pending)pthread_cond_wait(&cond_,&lock_);
			if(stop_)break;
			const uint32_t tag=tag_;
			state_=Working;
			pthread_mutex_unlock(&lock_);
			const bool ok=read_(ctx_,tag,buf_);
			pthread_mutex_lock(&lock_);
			state_=ok?Ready:Empty;
			if(!ok)stats_.failed++;
			pthread_cond_broadcast(&cond_);
		}
		pthread_mutex_unlock(&lock_);
	}
public:
	Worker()=default;
	Worker(const Worker&)=delete;
	Worker &operator=(const Worker&)=delete;
	~Worker() { Stop(); pthread_cond_destroy(&cond_); pthread_mutex_destroy(&lock_); }
	bool Start(size_t bytes,Reader read,void *ctx) {
		if(started_ || !bytes || !read)return false;
		buf_=(uint8_t*)malloc(bytes);
		if(!buf_)return false;
		bytes_=bytes;read_=read;ctx_=ctx;stop_=false;state_=Empty;stats_={};
		if(pthread_create(&thread_,nullptr,Entry,this)) {free(buf_);buf_=nullptr;return false;}
		started_=true;return true;
	}
	void Stop() {
		if(!started_)return;
		pthread_mutex_lock(&lock_);stop_=true;
		pthread_cond_broadcast(&cond_);pthread_mutex_unlock(&lock_);
		pthread_join(thread_,nullptr);
		started_=false;free(buf_);buf_=nullptr;state_=Empty;
	}
	// CPU may join only the exact demanded hunk. A speculative read of
	// any other hunk never blocks demand I/O on its independent handle.
	bool Take(uint32_t tag,uint8_t *dest) {
		pthread_mutex_lock(&lock_);
		if(tag_==tag && state_==Working) {
			stats_.waited++;
			while(state_==Working)pthread_cond_wait(&cond_,&lock_);
		}
		bool hit=state_==Ready && tag_==tag;
		if(hit) {memcpy(dest,buf_,bytes_);state_=Empty;stats_.hits++;}
		else if(state_==Pending && tag_==tag){state_=Empty;stats_.discarded++;}
		pthread_mutex_unlock(&lock_);
		return hit;
	}
	void Offer(uint32_t tag) {
		pthread_mutex_lock(&lock_);
		if(state_!=Working && (state_==Empty || tag_!=tag)) {
			if(state_!=Empty)stats_.discarded++;
			tag_=tag;state_=Pending;stats_.issued++;
			pthread_cond_signal(&cond_);
		}
		pthread_mutex_unlock(&lock_);
	}
	Stats GetStats() {
		pthread_mutex_lock(&lock_);Stats s=stats_;pthread_mutex_unlock(&lock_);return s;
	}
};
}
#endif
#endif
