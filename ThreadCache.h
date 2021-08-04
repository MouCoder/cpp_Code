//线程缓存：每个线程独有，小于64k的内存直接在thread cache申请
#include"CommonResource.h"
#include"CentralControlCache.h"

class ThreadCache
{
public:
	//申请和释放内存
	void* Allocate(size_t memSize);
	void DeAllocate(void* mem,size_t memSize);

	//从中心缓存中获取多个内存对象
	void* GetMemFromCentalCache(size_t index, size_t memSize);

	// 释放对象时，链表过长时，回收内存回到中心缓存
	void ListTooLong(FreeList& list, size_t size);
private:
	FreeList _freelist[THREAD_FREELIST_SIZE];
};

static __declspec(thread) ThreadCache* tls_threadcache = nullptr;