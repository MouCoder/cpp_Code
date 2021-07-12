#pragma once

#include "Common.h"

class ThreadCache
{
public:
	// 申请和释放内存对象
	void* Allocate(size_t memSize);
	void Deallocate(void* ptr, size_t memSize);

	// 从中心缓存获取对象
	void* GetMemoryFromCentralCache(size_t index, size_t memSize);

	// 释放对象时，链表过长时，回收内存回到中心缓存
	void ListTooLong(FreeList& list, size_t size);
private:
	FreeList _freeLists[NFREELISTS];
};

static __declspec(thread) ThreadCache* tls_threadcache = nullptr;
