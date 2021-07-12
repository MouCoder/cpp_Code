#include "MemoryManagment.h"

void func1()
{
	for (size_t i = 0; i < 10; ++i)
	{
		hcAlloc(17);
	}
}

void func2()
{
	for (size_t i = 0; i < 20; ++i)
	{
		hcAlloc(5);
	}
}

//测试多线程
void TestThreads()
{
	std::thread t1(func1);
	std::thread t2(func2);


	t1.join();
	t2.join();
}

//计算索引
void TestSizeClass()
{
	cout << SizeClass::Index(1035) << endl;
	cout << SizeClass::Index(1025) << endl;
	cout << SizeClass::Index(1024) << endl;
}

//申请内存
void TestConcurrentAlloc()
{
	void* ptr0 = hcAlloc(5);
	void* ptr1 = hcAlloc(8);
	void* ptr2 = hcAlloc(8);
	void* ptr3 = hcAlloc(8);

	hcFree(ptr1);
	hcFree(ptr2);
	hcFree(ptr3);
}

//大块内存的申请
void TestBigMemory()
{
	void* ptr1 = hcAlloc(65 * 1024);
	hcFree(ptr1);

	void* ptr2 = hcAlloc(129 * 4 * 1024);
	hcFree(ptr2);
}

//int main()
//{
//	//TestBigMemory();
//
//	//TestObjectPool();
//	//TestThreads();
//	//TestSizeClass();
//	//TestConcurrentAlloc();
//
//	return 0;
//}