#include "PageCache.h"

PageCache PageCache::_sInst;

// 向系统申请k页内存
void* PageCache::SystemAllocPage(size_t k)
{
	return ::SystemAlloc(k);
}


Span* PageCache::NewSpan(size_t k)
{
	//加锁
	std::lock_guard<std::recursive_mutex> lock(_mtx);

	// 针对直接申请大于NPAGES的大块内存，直接找系统要
	if (k >= NPAGES)
	{
		void* ptr = SystemAllocPage(k);
		Span* span = new Span;
		span->_pageId = (ADDRES_INT)ptr >> PAGE_SHIFT;
		span->_n = k;

		_idSpanMap[span->_pageId] = span;
		return span;
	}

	//如果有直接返回
	if (!_spanList[k].Empty())
	{
		return _spanList[k].PopFront();
	}

	//继续向后找一个大的进行切分
	for (size_t i = k + 1; i < NPAGES; ++i)
	{
		// 大页给切小,切成k页的span返回
		// 切出i-k页挂回自由链表
		if (!_spanList[i].Empty())
		{
			// 尾切出一个k页span
			Span* span = _spanList[i].PopFront();

			Span* split = new Span;
			split->_pageId = span->_pageId + span->_n - k;
			split->_n = k;

			// 改变切出来span的页号和span的映射关系
			for (PageID i = 0; i < k; ++i)
			{
				_idSpanMap[split->_pageId + i] = split;
			}

			span->_n -= k;
			//将span剩下的页挂在对应的位置
			_spanList[span->_n].PushFront(span);
			return split;
		}
	}
	//spanlist中没有内存，向系统申请一个128页的内存进行切割
	Span* bigSpan = new Span;
	void* memory = SystemAllocPage(NPAGES - 1);
	//计算出页号
	bigSpan->_pageId = (size_t)memory >> 12;
	bigSpan->_n = NPAGES - 1;
	// 按页号和span映射关系建立
	for (PageID i = 0; i < bigSpan->_n; ++i)
	{
		PageID id = bigSpan->_pageId + i;
		_idSpanMap[id] = bigSpan;
	}

	_spanList[NPAGES - 1].Insert(_spanList[NPAGES - 1].Begin(), bigSpan);

	return NewSpan(k);
}

Span* PageCache::MapObjectToSpan(void* obj)
{
	//std::lock_guard<std::recursive_mutex> lock(_mtx);
	//通过内存地址在map中找到对应的span
	PageID id = (ADDRES_INT)obj >> PAGE_SHIFT;

	Span* span = _idSpanMap.get(id);
	if (span != nullptr)
	{
		return span;
	}
	else
	{
		assert(false);
		return nullptr;
	}
}

void PageCache::ReleaseSpanToPageCache(Span* span)
{
	//大于128页的内存，直接还给系统
	if (span->_n >= NPAGES)
	{
		_idSpanMap.erase(span->_pageId);
		void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
		SystemFree(ptr);
		delete span;
		return;
	}

	std::lock_guard<std::recursive_mutex> lock(_mtx);

	// 检查前后空闲span页，进行合并,解决内存碎片问题

	// 向前合并
	while (1)
	{
		PageID preId = span->_pageId - 1;
		Span* preSpan = _idSpanMap.get(preId);
		if (preSpan == nullptr)
		{
			break;
		}

		// 如果前一个页的span还在使用中，结束向前合并
		if (preSpan->_usecount != 0)
		{
			break;
		}

		// 开始合并...

		// 超过128页，不需要合并了
		if (preSpan->_n + span->_n >= NPAGES)
		{
			break;
		}

		// 从对应的span链表中解下来，再合并
		_spanList[preSpan->_n].Erase(preSpan);

		span->_pageId = preSpan->_pageId;
		span->_n += preSpan->_n;

		// 更新页之间映射关系
		for (PageID i = 0; i < preSpan->_n; ++i)
		{
			_idSpanMap[preSpan->_pageId + i] = span;
		}

		delete preSpan;
	}

	// 向后合并
	while (1)
	{
		//得到后一个页
		PageID nextId = span->_pageId + span->_n;

		//通过页号在map中查找后一个页对应的span
		Span* nextSpan = _idSpanMap.get(nextId);
		if (nextSpan == nullptr)
		{
			break;
		}

		//没有使用完，不能合并
		if (nextSpan->_usecount != 0)
		{
			break;
		}

		// 超过128页，不需要合并了
		if (nextSpan->_n + span->_n >= NPAGES)
		{
			break;
		}

		//删除原来页的映射关系
		_spanList[nextSpan->_n].Erase(nextSpan);

		span->_n += nextSpan->_n;

		//对合并完成后的页重新建立映射关系
		for (PageID i = 0; i < nextSpan->_n; ++i)
		{
			_idSpanMap[nextSpan->_pageId + i] = span;
		}

		delete nextSpan;
	}

	// 合并出的大span，插入到对应的链表中
	_spanList[span->_n].PushFront(span);
}