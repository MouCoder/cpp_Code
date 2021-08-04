#include"PageCache.h"

//类内声明，类外初始化
PageCache PageCache::_sInst;

// 向系统申请k页内存
void* PageCache::SystemAllocPage(size_t k)
{
	return ::SystemAlloc(k);
}


//申请num页的内存
Span* PageCache::NewSpan(size_t num)
{
	std::lock_guard<std::recursive_mutex> lock(_mtx);
	//针对直接申请大于128页的内存，直接向系统要
	if (num >= 128)
	{
		//在系统申请
		void* ptr = SystemAllocPage(num);
		Span* span = new Span;
		span->_pageId = (ADDRES_INT)ptr >> PAGE_SHIFT;
		span->_n = num;

		{
			//std::lock_guard<std::mutex> lock(_map_mtx);
			_idSpanMap[span->_pageId] = span;
		}

		return span;
	}
	else
	{
		if (!_spanList[num - 1].isEmpty())
			_spanList[num - 1].isEmpty();
		//走到这里说明没有内存，需要向后判断是否有num页的内存

		//向后找
		for (int i = num; i < 128; i++)
		{
			if (!_spanList[i].isEmpty())
			{
				//切num页给_spanList[num - 1]
				Span* span = _spanList[i].PopFront();
					
				//切出来的内存---采用尾切
				Span* split = new Span;
				split->_n = num;
				split->_next = nullptr;
				//span中有多页，它存储的页号是第一页的页号，因此这里的页号应该是切出来的页数+初始页号
				split->_pageId = span->_pageId+span->_n-num;

				// 改变切出来span的页号和span的映射关系
				{
					//std::lock_guard<std::mutex> lock(_map_mtx);
					for (PageID i = 0; i < num; ++i)
					{
						_idSpanMap[split->_pageId + i] = split;
					}
				}

				//将Span剩余的页挂在对应的位置
				span->_n -= num;
				_spanList[span->_n - 1].PushFront(span);

				return split;
			}
		}
		
		//走到这里说明page cache中没有内存,向系统申请
		Span* bigSpan = new Span;
		void* memory = SystemAllocPage(MAX_PAGE);
		//计算出页号
		bigSpan->_pageId = (size_t)memory >> 12;
		bigSpan->_n = MAX_PAGE;
		// 按页号和span映射关系建立，也就是这么多个页都映射在这个span中
		{
			//std::lock_guard<std::mutex> lock(_map_mtx);
			for (PageID i = 0; i < bigSpan->_n; ++i)
			{
				PageID id = bigSpan->_pageId + i;
				_idSpanMap[id] = bigSpan;
			}
		}
		//将内存链接到spanlist中
		_spanList[MAX_PAGE - 1].Insert(_spanList[MAX_PAGE - 1].Begin(), bigSpan);
		//重新切割
		return NewSpan(num);
	}
}

//通过内存地址在map中查找对应的span
Span* PageCache::GetSpanToMap(void* mem)
{
	//根据内存计算页号
	PageID id = (ADDRES_INT)mem >> PAGE_SHIFT;
	//在map中查找
	auto ret = _idSpanMap.get(id);
	if (ret != nullptr)
	{
		return ret;
	}
	else
	{
		assert(false);
		return  nullptr;
	}
}

// 释放空闲span回到Pagecache，并合并相邻的span
void PageCache::ReleaseSpanToPageCache(Span* span)
{
	if (span->_n >= MAX_PAGE)
	{
		//大于128页的内存直接还给系统内存
		_idSpanMap.erase(span->_pageId);
		//根据页号计算地址
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
		if (preSpan->_n + span->_n >= MAX_PAGE)
		{
			break;
		}

		// 从对应的span链表中解下来，再合并
		_spanList[preSpan->_n].Erase(preSpan);

		span->_pageId = preSpan->_pageId;
		span->_n += preSpan->_n;

		// 更新页之间映射关系
		{
			//std::lock_guard<std::mutex> lock(_map_mtx);
			for (PageID i = 0; i < preSpan->_n; ++i)
			{
				_idSpanMap[preSpan->_pageId + i] = span;
			}
		}

		delete preSpan;
	}

	// 向后合并
	while (1)
	{
		PageID nextId = span->_pageId + span->_n;

		Span* nextSpan = _idSpanMap.get(nextId);
		if (nextSpan == nullptr)
		{
			break;
		}

		//Span* nextSpan = ret->second;
		if (nextSpan->_usecount != 0)
		{
			break;
		}

		// 超过128页，不需要合并了
		if (nextSpan->_n + span->_n >= MAX_PAGE)
		{
			break;
		}

		_spanList[nextSpan->_n].Erase(nextSpan);

		span->_n += nextSpan->_n;

		{
			//std::lock_guard<std::mutex> lock(_map_mtx);
			for (PageID i = 0; i < nextSpan->_n; ++i)
			{
				_idSpanMap[nextSpan->_pageId + i] = span;
			}
		}

		delete nextSpan;
	}

	// 合并出的大span，插入到对应的链表中
	_spanList[span->_n].PushFront(span);
}