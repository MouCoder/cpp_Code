#pragma once
#include "Common.h"
class CentralControlCache
{
public:
	static CentralControlCache* GetInstance()
	{
		return &_sInst;
	}

	// 从中心缓存获取一定数量的对象给thread cache
	size_t FetchRangeObj(void*& start, void*& end, size_t n, size_t memSize);

	// 从SpanList或者page cache获取一个span
	Span* GetOneSpan(SpanList& list, size_t memSize);

	// 将一定数量的对象释放到span跨度
	void ReleaseListToSpans(void* start, size_t memSize);
private:
	SpanList _spanLists[NFREELISTS]; // 按对齐方式映射

private:
	//单利模式设计
	CentralControlCache()
	{}
	//delete表示不能在外边实现
	CentralControlCache(const CentralControlCache&) = delete;

	static CentralControlCache _sInst;
};

