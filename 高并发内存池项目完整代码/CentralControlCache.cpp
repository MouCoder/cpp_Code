#include "CentralControlCache.h"
#include "PageCache.h"

//单例，static类型类内声明类外初始化
CentralControlCache CentralControlCache::_sInst;

Span* CentralControlCache::GetOneSpan(SpanList& list, size_t memSize)
{
	// 现在spanlist中去找还有内存的span
	Span* it = list.Begin();
	while (it != list.End())
	{
		if (it->_list)
		{
			return it;
		}

		it = it->_next;
	}

	// 走到这里代表着span都没有内存了，只能找pagecache
	Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(memSize));
	// 切分好挂在list中
	char* start = (char*)(span->_pageId << PAGE_SHIFT);
	char* end = start + (span->_n << PAGE_SHIFT);
	while (start < end)
	{
		char* next = start + memSize;
		// 头插
		NextObj(start) = span->_list;
		span->_list = start;

		start = next;
	}
	span->_objsize = memSize;

	list.PushFront(span);

	return span;
}

size_t CentralControlCache::FetchRangeObj(void*& start, void*& end, size_t n, size_t memSize)
{
	//根据内存对象大小，计算索引
	size_t i = SizeClass::Index(memSize);

	//桶锁
	std::lock_guard<std::mutex> lock(_spanLists[i]._mtx);

	Span* span = GetOneSpan(_spanLists[i], memSize);
	// 找到一个有对象的span，有多少给多少
	size_t j = 1;
	start = span->_list;
	void* cur = start;
	void* prev = start;
	while (j <= n && cur != nullptr)
	{
		prev = cur;
		cur = NextObj(cur);
		++j;
		span->_usecount++;
	}

	span->_list = cur;
	end = prev;
	NextObj(prev) = nullptr;
	//返回实际申请到的内存个数
	return j - 1;
}

void CentralControlCache::ReleaseListToSpans(void* start, size_t memSize)
{
	//根据内存对象大小计算索引
	size_t i = SizeClass::Index(memSize);
	std::lock_guard<std::mutex> lock(_spanLists[i]._mtx);

	//将每一个内存对象插入到自己对应的span中
	while (start)
	{
		void* next = NextObj(start);

		// 找start内存块属于哪个span
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);

		// 把对象插入到span管理的list中
		NextObj(start) = span->_list;
		span->_list = start;
		span->_usecount--;
		// _usecount == 0说明这个span中切出去的大块内存
		// 都还回来了，在还给page cache合并成更大的内存
		if (span->_usecount == 0)
		{
			_spanLists[i].Erase(span);
			span->_list = nullptr;
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		}

		start = next;
	}
}
