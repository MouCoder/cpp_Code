#pragma once
#include "Common.h"
#include "PageMap.h"

class PageCache
{
public:
	static PageCache* GetInstance()
	{
		return &_sInst;
	}

	// 向系统申请k页内存挂到自由链表
	void* SystemAllocPage(size_t k);

	Span* NewSpan(size_t k);

	// 获取从对象到span的映射
	Span* MapObjectToSpan(void* obj);

	// 释放空闲span回到Pagecache，并合并相邻的span
	void ReleaseSpanToPageCache(Span* span);
private:
	SpanList _spanList[NPAGES];	// 按页数映射

	// tcmalloc 基数树  效率更高
	TCMalloc_PageMap2<32 - PAGE_SHIFT> _idSpanMap;

	std::recursive_mutex _mtx;


private:
	PageCache()
	{}

	PageCache(const PageCache&) = delete;

	// 单例
	static PageCache _sInst;
};
