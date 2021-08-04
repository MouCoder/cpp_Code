//公共资源：包含一些公共的头文件以及公共类

#pragma once
#include<iostream>
#include<assert.h>
#include <exception>
#include <time.h>
#include <thread>
#include <mutex>
#include <windows.h>
#include <cstddef>

//页号---32位系统使用size_t,64位系统使用long long
#ifdef _WIN32
	typedef size_t PageID;
#else
	typedef unsigned long long PageID;
#endif // _WIN32

#ifdef _WIN32
	typedef size_t ADDRES_INT;
#else
	typedef unsigned long long ADDRES_INT;
#endif // _WIN32

using std::cout;
using std::cin;

static const size_t THREAD_MAX_SIZE = 64 * 1024;   //thread cache的能够申请的最大内存大小为64k
static const size_t THREAD_FREELIST_SIZE = 184;  //thread cache中的freelist数组的大小
static const size_t CENTRAL_SPANLIST_SIZE = 184; //central control cache中spanlist的大小
static const size_t PAGE_SHIFT = 12;//一页的大小为2^PAGE_SHIFT
static const size_t MAX_PAGE = 128;//page cache中最多的页数

inline void*& Next(void* mem)
{
	return *((void**)mem);
}

//返回两个数中的较小者
static size_t min(size_t num1, size_t num2)
{
	return num1 < num2 ? num1 : num2;
}

class SizeCalculation
{
	// 控制在12%左右的内碎片浪费
	// [1,128]					8byte对齐	 freelist[0,16)
	// [129,1024]				16byte对齐	 freelist[16,72)
	// [1025,8*1024]			128byte对齐	 freelist[72,128)
	// [8*1024+1,64*1024]		1024byte对齐  freelist[128,184)
public:
	//根据内存大小和对齐数计算对应下标
	static inline size_t _Intex(size_t size, size_t alignmentShift)
	{
		//alignmentShift表示对齐数的位数，例如对齐数为8 = 2^3时，aligmentShift = 3
		//这样可以将除法转化成>>运算，提高运算效率
		return ((size + (1 << alignmentShift) - 1) >> alignmentShift) - 1;
	}

	//根据内存大小，计算对应的下标
	static inline size_t Index(size_t size)
	{
		assert(size <= THREAD_MAX_SIZE);

		//每个对齐数对应的索引个数，分别表示8 16 128 1024字节对齐
		int groupArray[4] = {0,16,56,56};

		if (size <= 128)
		{
			//8字节对齐
			return _Intex(size, 3) + groupArray[0];
		}
		else if (size <= 1024)
		{
			//16字节对齐
			return _Intex(size, 4) + groupArray[1];
		}
		else if (size <= 8192)
		{
			//128字节对齐
			return _Intex(size, 7) + groupArray[2];
		}
		else if (size <= 65536)
		{
			//1024字节对齐
			return _Intex(size, 10) + groupArray[3];
		}

		assert(false);
		return -1;
	}

	static inline size_t _RoundUp(size_t size, size_t alignment)
	{
		//子函数，传入大小和对齐数进行对齐
		return (size + alignment - 1) & ~(alignment - 1);
	}

	//将申请的内存大小进行向上对齐
	static inline size_t RoundUp(size_t size)
	{
		if (size <= 128)
		{
			//8字节对齐
			return _RoundUp(size,8);
		}
		else if (size <= 1024)
		{
			//16字节对齐
			return _RoundUp(size, 16);
		}
		else if (size <= 8192)
		{
			//128字节对齐
			return _RoundUp(size, 128);
		}
		else if (size <= 65536)
		{
			//1024字节对齐
			return _RoundUp(size, 1024);
		}
		else
			return _RoundUp(size, 1 << PAGE_SHIFT);

		return -1;
	}

	static inline size_t NumMoveSize(size_t memSize)
	{
		if (memSize == 0)
			return 0;

		// [2, 512]，一次批量移动多少个对象的(慢启动)上限值
		// 小对象一次批量上限高
		// 大对象一次批量上限低，最少一次给俩
		size_t num = THREAD_MAX_SIZE / memSize;
		if (num < 2)
			num = 2;

		if (num > 512)
			num = 512;

		return num;
	}

	//计算申请多少页内存
	static inline size_t NumMovePage(size_t memSize)
	{
		//计算thread cache最多申请多少个对象，这里就给多少个对象
		size_t num = NumMoveSize(memSize);
		//此时的nPage表示的是获取的内存大小
		size_t nPage = num*memSize;
		//当npage右移是PAGE_SHIFT时表示除2的PAGE_SHIFT次方，表示的就是页数
		nPage >>= PAGE_SHIFT;

		//最少给一页（体现了按页申请的原则）
		if (nPage == 0)
			nPage = 1;

		return nPage;
	}
};

//自由链表，用来将小块内存对象链接起来进行管理
class FreeList
{
public:
	//判断链表是否为空
	bool empty()
	{
		return _head == nullptr;
	}

	void* pop()
	{
		assert(_head != nullptr);
		//头删
		void* mem = _head;
		_head = Next(_head);

		_size--;
		return mem;
	}

	//获取一批对象
	void PopRange(void*& start,void*& end,size_t num)
	{
		//这里保证一定有num个对象（前边释放内存前已经判断过了）
		start = _head;
		for (size_t i = 0; i < num; ++i)
		{
			end = _head;
			_head = Next(_head);
		}

		Next(end) = nullptr;
		_size -= num;
	}

	void push(void* mem)
	{
		//头插
		Next(mem) = _head;
		_head = mem;

		_size++;
	}

	void PushRange(void* start, void* end, size_t size)
	{
		//插入一批对象
		Next(end) = _head;
		_head = start;

		_size += size;
	}

	size_t MaxSize()
	{
		return _maxSize;
	}

	size_t Size()
	{
		return _size;
	}

	void SetMaxSize(size_t newVal)
	{
		_maxSize = newVal;
	}
private:
	void* _head = nullptr; //链表头节点

	size_t _maxSize = 1;//每次获取的内存对象个数，最大到512
	size_t _size = 0;//链表中对象的个数
};

struct Span
{
	PageID _pageId = 0;   // 页号
	size_t _n = 0;        // 页的数量

	Span* _next = nullptr;
	Span* _prev = nullptr;

	void* _list = nullptr;  // 大块内存切小链接起来，这样回收回来的内存也方便链接
	size_t _usecount = 0;	// 使用计数，==0 说明所有对象都回来了

	size_t _objsize = 0;	// 切出来的单个对象的大小
};

//管理大块内存---双向带头循环链表
class SpanList
{
public:
	SpanList()
	{
		_head = new Span;
		_head->_next = _head;
		_head->_prev = _head;
	}
	bool isEmpty()
	{
		return _head->_next == _head;
	}
	
	Span* Begin()
	{
		//带头双向循环链表表头是_head->next
		return _head->_next;
	}

	Span* End()
	{
		//带头双向循环链表，表尾指针是_head
		return _head;
	}

	//头删
	Span* PopFront()
	{
		Span* tmp = _head->_next;
		//从SpanList中删除节点
		Erase(tmp);
		return tmp;
	}
	//头插
	void PushFront(Span* newSpan)
	{
		Insert(Begin(), newSpan);
	}

	//删除一个span
	void Erase(Span* cur)
	{
		assert(cur != _head);

		//双向带头循环链表的删除
		Span* prev = cur->_prev;
		Span* next = cur->_next;

		prev->_next = next;
		next->_prev = prev;
	}

	//在cur位置之前插入一个节点newspan
	void Insert(Span* cur, Span* newspan)
	{
		Span* prev = cur->_prev;
		prev->_next = newspan;
		newspan->_prev = prev;

		newspan->_next = cur;
		cur->_prev = newspan;
	}
private:
	Span* _head;
public:
	std::mutex _mtx;
};

//向系统申请KPage的内存
static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage*(1 << PAGE_SHIFT),
		MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	// brk mmap等
#endif
	//申请失败抛异常
	if (ptr == nullptr)
		throw std::bad_alloc();

	return ptr;
}
//释放内存
static void SystemFree(void* mem)
{
	free(mem);
}