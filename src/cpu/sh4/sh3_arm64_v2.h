// CV1000 dynarec V2, stage A. Independently implemented cache/frontend;
// reuse the audited straight-line V1 instruction emitter until V2 IR.
// Never map a page with both WRITE and EXEC permission.
#ifndef FBNEO_SH3_ARM64_V2_H
#define FBNEO_SH3_ARM64_V2_H
#include "sh3_arm64_native.h"
#if SH3_NATIVE && defined(LSB_FIRST) && !defined(__ANDROID__)
#include <sys/syscall.h>
#include <new>
#if defined(SYS_memfd_create)
#define SH3_NATIVE_V2 1
#else
#define SH3_NATIVE_V2 0
#endif
#else
#define SH3_NATIVE_V2 0
#endif

#if SH3_NATIVE_V2
namespace Sh3V2 {
class CodeCache {
	uint8_t *write_, *exec_;
	size_t used_, capacity_;
	bool denied_;
	CodeCache(const CodeCache&) = delete;
	CodeCache& operator=(const CodeCache&) = delete;
	bool Open() {
		const long page=sysconf(_SC_PAGESIZE);
		if(page<=0 || !capacity_ || capacity_%size_t(page) ||
			capacity_>UINT32_MAX)return false;
		// MFD_CLOEXEC; no executable-file or security-policy workaround.
		const int fd=int(syscall(SYS_memfd_create,"fbneo-sh3-v2",1u));
		if(fd<0)return false;
		if(ftruncate(fd,off_t(capacity_))){close(fd);return false;}
		void *rw=mmap(NULL,capacity_,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
		if(rw==MAP_FAILED){close(fd);return false;}
		void *rx=mmap(NULL,capacity_,PROT_READ|PROT_EXEC,MAP_SHARED,fd,0);
		close(fd);
		if(rx==MAP_FAILED){munmap(rw,capacity_);return false;}
		write_=static_cast<uint8_t*>(rw);exec_=static_cast<uint8_t*>(rx);
		return true;
	}
public:
	static const size_t Capacity=8u*1024u*1024u;
	explicit CodeCache(size_t capacity=Capacity)
		:write_(NULL),exec_(NULL),used_(0),capacity_(capacity),denied_(false){}
	~CodeCache(){Reset();}
	void Reset(){
		if(write_)munmap(write_,capacity_);
		if(exec_)munmap(exec_,capacity_);
		write_=exec_=NULL;used_=0;denied_=false;
	}
	bool Publish(const uint32_t *words,size_t count,uint32_t &offset){
		if(!count || denied_ || count>capacity_/sizeof(uint32_t))return false;
		const size_t bytes=count*sizeof(uint32_t);
		// Never modify a published block. Full cache degrades to the
		// interpreter; do not reset/reuse executable addresses mid-game.
		const size_t padded=(bytes+15u)&~size_t(15u);
		if(padded>capacity_-used_)return false;
		if(!write_ && !Open()){denied_=true;return false;}
		uint8_t *rw=write_+used_,*rx=exec_+used_;
		memcpy(rw,words,bytes);
		// AArch64: clean the writing alias before invalidating the
		// executable alias. Both builtins include the required barriers.
		// Never execute through RW, and never write through RX.
		__builtin___clear_cache(reinterpret_cast<char*>(rw),reinterpret_cast<char*>(rw+bytes));
		__builtin___clear_cache(reinterpret_cast<char*>(rx),reinterpret_cast<char*>(rx+bytes));
		offset=uint32_t(used_);used_+=padded;return true;
	}
	unsigned Run(uint32_t offset,uint32_t *r,uint32_t *sr,uint32_t *ea,
		const Sh3Native::Memory &memory)const{
		return reinterpret_cast<unsigned (*)(uint32_t*,uint32_t*,uint32_t*,const Sh3Native::Memory*)>(exec_+offset)(r,sr,ea,&memory);
	}
	bool Denied()const{return denied_;}
	size_t Used()const{return used_;}
	const void *WriteBase()const{return write_;}
	const void *ExecBase()const{return exec_;}
};
}
#endif
#endif
