//中心控制缓存：承上启下，调度内存
#pragma once
#include"CentralControlCache.h"
#include"PageCache.h"

//类内声明，类外初始化
CentralControlCache CentralControlCache::_sInst;

size_t CentralControlCache::GetRangeMem(void*& start, void*& end, size_t expectedNum, size_t memSize)
{
	//计算memSize大小的内存对象在SpanList的索引
	size_t index = SizeCalculation::Index(memSize);

	std::lock_guard<std::mutex> lock(_spanlists[index]._mtx);
	//在SpanList中获取还有内存的内存的Span
	Span* sp = GetOneSpan(_spanlists[index], memSize);

	// 找到一个有对象的span，有多少给多少
	size_t j = 1;
	start = sp->_list;
	void* cur = start;
	void* prev = start;
	while (j <= expectedNum && cur != nullptr)
	{
		prev = cur;
		cur = Next(cur);
		++j;
		//使用数量+1，方便归还内存时判断是否全部归还
		sp->_usecount++;
	}

	sp->_list = cur;
	end = prev;
	Next(prev) = nullptr;
	//实际申请的对象个数为j-1个
	return j - 1;
}

//在SpanList中获取还有内存的内存的Span
Span* CentralControlCache::GetOneSpan(SpanList& list, size_t memSize)
{
	//遍历SpanList，判断是否有内存
	Span* cur = list.Begin();
	while (cur != list.End())
	{
		if (cur->_list == nullptr)
			cur = cur->_next;
		else
			return cur;
	}

	//走到这里证明没有有内存的Span,找pagecache申请内存
	Span* span = PageCache::GetInstance()->NewSpan(SizeCalculation::NumMovePage(memSize));

	//将申请到的Span切割好挂在SpanList中
	//根据页号计算其实地址
	char* start = (char*)(span->_pageId << PAGE_SHIFT);
	//根据开始地址和页数计算最后一页的二地址
	char* end = start + (span->_n << PAGE_SHIFT);
	while (start < end)
	{
		char* next = start + memSize;
		// 头插
		Next(start) = span->_list;
		span->_list = start;

		start = next;
	}
	//设置单个对象的大小
	span->_objsize = memSize;
	//将申请到的span链接到spanlist中
	list.PushFront(span);
	return span;
}

void CentralControlCache::ReleaseListToSpans(void* start, size_t memSize)
{
	size_t index = SizeCalculation::Index(memSize);
	std::lock_guard<std::mutex> lock(_spanlists[index]._mtx);

	//将一个个对象还给_spanlist中对应的span
	while (start)
	{
		void* next = Next(start);

		// 找start内存块属于哪个span
		Span* span = PageCache::GetInstance()->GetSpanToMap(start);

		// 把对象插入到span管理的list中
		Next(start) = span->_list;
		span->_list = start;
		span->_usecount--;
		// _usecount == 0说明这个span中切出去的大块内存都还回来了
		if (span->_usecount == 0)
		{
			//将这个span还给pagecache
			_spanlists[index].Erase(span);
			span->_list = nullptr;
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		}

		start = next;
	}
}
