#pragma once
#include"CommonResource.h"

class CentralControlCache
{
public:
	static CentralControlCache* GetInstance()
	{
		return &_sInst;
	};
	//获取多个内存对象--开始地址 结束地址 期望个数 内存对象大小,返回实际获取的内存对象个数
	size_t GetRangeMem(void*& start, void*& end, size_t expectedNum, size_t size);
	//在SpanList中获取还有内存的内存的Span
	Span* GetOneSpan(SpanList& list,size_t size);

	//释放内存到central control cache中
	void ReleaseListToSpans(void* start,size_t memSize);
private:
	SpanList _spanlists[CENTRAL_SPANLIST_SIZE];
private:
	//单例设计
	CentralControlCache()
	{}

	CentralControlCache(const CentralControlCache&) = delete;
	//因为是static类型的，因此可以不适用指针类型
	static CentralControlCache _sInst;
};