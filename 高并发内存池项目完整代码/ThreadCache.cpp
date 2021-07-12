#include "ThreadCache.h"
#include "CentralControlCache.h"

void* ThreadCache::GetMemoryFromCentralCache(size_t index, size_t memSize)
{
	// 获取一批对象，数量使用慢启动方式
	// SizeClass::NumMoveSize(size)是上限值,在最大值和maxsize中取较小值
	//这里是期望获取的对象个数
	size_t expectNum = min(SizeClass::NumMoveSize(memSize), _freeLists[index].MaxSize());

	// 去中心缓存获取batch_num个对象
	void* start = nullptr;
	void* end = nullptr;
	//这里会返回实际获取到的内存个数，至少应该为1个
	size_t actualNum = CentralControlCache::GetInstance()->FetchRangeObj(start, end, expectNum, SizeClass::RoundUp(memSize));
	assert(actualNum > 0);

	// >1，返回一个，剩下挂到自由链表
	// 如果一次申请多个，剩下挂起来，下次申请就不需要找中心缓存，减少锁竞争，提高效率
	if (actualNum > 1)
	{
		_freeLists[index].PushRange(NextObj(start), end, actualNum - 1);
	}

	// 慢启动增长
	if (_freeLists[index].MaxSize() == expectNum)
	{
		_freeLists[index].SetMaxSize(_freeLists[index].MaxSize() + 1);
	}

	return start;
}

void* ThreadCache::Allocate(size_t memSize)
{
	//根据申请内存的大小计算在freelist中的索引
	size_t index = SizeClass::Index(memSize);
	if (!_freeLists[index].Empty())
	{
		//从freelist中有内存，直接返回一个内存对象
		return _freeLists[index].Pop();
	}
	else
	{
		//如果freelist中没有内存直接去centralcontrolcache申请
		return GetMemoryFromCentralCache(index, memSize);
	}
}

void ThreadCache::Deallocate(void* ptr, size_t memSize)
{
	//通过单个对象大小计算在freelist中的索引
	size_t i = SizeClass::Index(memSize);
	_freeLists[i].Push(ptr);

	// 当list中的内存太多去释放搭配central control cache中
	if (_freeLists[i].Size() >= _freeLists[i].MaxSize())
	{
		ListTooLong(_freeLists[i], memSize);
	}
}

// 释放对象时，链表过长时，回收内存回到中心缓存
void ThreadCache::ListTooLong(FreeList& list, size_t memSize)
{
	size_t expectNum = list.MaxSize();
	void* start = nullptr;
	void* end = nullptr;
	//从链标中将这些内存删除
	list.PopRange(start, end, expectNum);
	//还给central control cache中
	CentralControlCache::GetInstance()->ReleaseListToSpans(start, memSize);
}