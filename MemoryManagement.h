//内存管理：直接向用户提供申请内存和释放内存的接口
#pragma once
#include"CommonResource.h"
#include"ThreadCache.h"
#include"PageCache.h"

//high concurrent memory pool
static void* hcMalloc(size_t memSize)
{
	try
	{
		if (memSize > THREAD_MAX_SIZE)
		{
			//向page cache申请内存
			size_t npage = SizeCalculation::RoundUp(memSize) >> PAGE_SHIFT;
			Span* span = PageCache::GetInstance()->NewSpan(npage);
			span->_objsize = memSize;

			void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
			return ptr;
		}
		else
		{
			//创建tls,每个线程独有thread cache，向thread cache申请
			if (tls_threadcache == nullptr)
			{
				tls_threadcache = new ThreadCache;
			}

			return tls_threadcache->Allocate(memSize);
		}
	}
	catch (const std::exception& e)
	{
		cout << e.what() << std::endl;
	}
	return nullptr;
}

static void hcFree(void* mem)
{
	try{
		//根据内存大小在map中查找对应的span
		Span* sp = PageCache::GetInstance()->GetSpanToMap(mem);
		//获取mem的大小
		size_t memSize = sp->_objsize;

		if (memSize > THREAD_MAX_SIZE)
		{
			//还给pagecache
			PageCache::GetInstance()->ReleaseSpanToPageCache(sp);
		}
		else
		{
			//还给threadcache
			assert(tls_threadcache);
			tls_threadcache->DeAllocate(mem, memSize);
		}
	}
	catch (const std::exception& e)
	{
		cout << e.what() << std::endl;
	}
}