#include"ThreadCache.h"

//从central control cache中获取一块内存
void* ThreadCache::GetMemFromCentalCache(size_t index,size_t memSize)
{
	//central control cache中内存对象对应的索引和freelist中对应的索引应该是相同，直接由参数传过来，减少计算

	//使用慢启动获取一批内存对象
	size_t expectedNum = min(SizeCalculation::NumMoveSize(memSize), _freelist[index].MaxSize());//期望申请的内存对象个数

	//去中心缓存中获取expectedNum个对象
	void* start = nullptr;
	void* end = nullptr;
	//这里对内存大小需要向上对齐
	size_t actualNum = CentralControlCache::GetInstance()->GetRangeMem(start, end, expectedNum, SizeCalculation::RoundUp(memSize));

	//申请失败
	assert(actualNum > 0);

	//如果您申请到的内存多余1个，将后边的挂起来，返回第一个
	if (actualNum > 1)
	{
		_freelist[index].PushRange(Next(start), end, actualNum - 1);
		//将start的next置空
		Next(start) = nullptr;
	}

	//慢启动增长：将最大值+1，下次申请时比这次申请的就多一个；否则表明已经到了最大值，就不在增加了
	if (expectedNum == _freelist[index].MaxSize())
		_freelist[index].SetMaxSize(expectedNum+1);
	
	//将第一个返回
	return start;
}

//申请内存
void* ThreadCache::Allocate(size_t memSize)
{
	assert(memSize <= THREAD_MAX_SIZE);
	//计算索引---内存对象在freelist数组中的索引
	size_t freelistIndex = SizeCalculation::Index(memSize);

	if (_freelist[freelistIndex].empty())
	{
		//在central control cache中申请
		return GetMemFromCentalCache(freelistIndex,memSize);
	}
	else
	{
		//直接从链表中取一个内存对象返回
		return _freelist[freelistIndex].pop();
	}
}
//释放内存
void ThreadCache::DeAllocate(void* mem, size_t memSize)
{
	//计算对应的索引
	size_t index = SizeCalculation::Index(memSize);
	//将mem内存Push到_freelist中
	_freelist[index].push(mem);

	//判断_freelist中的内存是否需要归还给centtral control cache中
	if (_freelist[index].Size() >= _freelist[index].MaxSize())
	{
		ListTooLong(_freelist[index], memSize);
	}
}

// 释放对象时，链表过长时，回收内存回到中心缓存
void ThreadCache::ListTooLong(FreeList& list, size_t memSize)
{
	size_t batchNum = list.MaxSize();
	void* start = nullptr;
	void* end = nullptr;
	//获取到maxsize个对象(_freelist中删除)
	list.PopRange(start, end, batchNum);
	//释放到central control cache中
	CentralControlCache::GetInstance()->ReleaseListToSpans(start, memSize);
}
