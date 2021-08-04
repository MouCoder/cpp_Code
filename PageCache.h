#pragma once
#include"CommonResource.h"
#include"PageMap.h"

class PageCache
{
public:
	static PageCache* GetInstance()
	{
		return &_sInst;
	}
	//申请num页的内存
	Span* NewSpan(size_t num);

	//直接向系统申请num页内存
	void* SystemAllocPage(size_t num);
	//通过内存地址在map中查找对应的span
	Span* GetSpanToMap(void* mem);
	// 释放空闲span回到Pagecache，并合并相邻的span
	void ReleaseSpanToPageCache(Span* span);
	std::recursive_mutex _mtx;
private:
	TCMalloc_PageMap2<32 - PAGE_SHIFT> _idSpanMap;
	SpanList _spanList[MAX_PAGE];
private:
	//单例模式
	PageCache()
	{}

	PageCache(const PageCache&) = delete;//delete的意思是不允许在别的地方定义

	// 单例
	static PageCache _sInst;
};