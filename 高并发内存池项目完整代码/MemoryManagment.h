#pragma once

#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"

//void* tcmalloc(size_t size)
static void* hcAlloc(size_t memSize)
{
	try
	{
		//大于64*1024字节的内存直接找pagecache申请（也就是直接在系统中申请）
		if (memSize > MAX_BYTES)
		{
			//将申请的内存向上对齐
			size_t npage = SizeClass::RoundUp(memSize) >> PAGE_SHIFT;
			//向pagecache申请内存（申请到的是一个span）
			Span* span = PageCache::GetInstance()->NewSpan(npage);
			span->_objsize = memSize;
			//使用页号计算内存地址
			void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
			return ptr;
		}
		else
		{
			//每个线程创建自己的threadcache
			if (tls_threadcache == nullptr)
			{
				tls_threadcache = new ThreadCache;
			}
			//去threadcache中申请内存
			return tls_threadcache->Allocate(memSize);
		}
	}
	catch (const std::exception& e)
	{
		cout << e.what() << endl;
	}
	return nullptr;
}

static void hcFree(void* ptr)
{
	try
	{
		//通过内存地址在map中找到对应的span
		Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
		size_t size = span->_objsize;

		if (size > MAX_BYTES)
		{
			//单个对象的大小大于64*1024字节直接释放到pagecache
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		}
		else
		{
			assert(tls_threadcache);
			//将内存挂到threadcache对应的freelist链表中
			tls_threadcache->Deallocate(ptr, size);
		}
	}
	catch (const std::exception& e)
	{
		cout << e.what() << endl;
	}
}