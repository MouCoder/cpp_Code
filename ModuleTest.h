#pragma once

#include"MemoryManagement.h"
#include<stdlib.h>
#include<vector>
//#include"ThreadCache.h"

void testAlloc()
{
	//ThreadCache* tc = new ThreadCache();
	srand((int)time(0));
	//申请内存
	/*void* mem = hcMalloc(8);
	cout << mem << std::endl;*/
	//申请513个小于等于8字节的内存
	/*for (int i = 0; i < 65537; i++)
	{
		size_t memSize = rand() % 1 + 8;
		cout <<i<<"  "<< hcMalloc(memSize) << std::endl;
	}*/

	//随意大小
	for (int i = 0; i < 6900; i++)
	{
		size_t memSize = rand() % 1 + 30 * 1024;
		cout << memSize << "  " << hcMalloc(memSize) << std::endl;
	}
}

void func1()
{
	for (size_t i = 0; i < 10; ++i)
	{
		hcMalloc(17);
	}
}

void func2()
{
	for (size_t i = 0; i < 20; ++i)
	{
		hcMalloc(5);
	}
}

//多线程
void TestThreads()
{
	std::thread t1(func1);
	std::thread t2(func2);


	t1.join();
	t2.join();
}

void TestSizeClass()
{
	cout << SizeCalculation::Index(1035) << std::endl;
	cout << SizeCalculation::Index(1025) << std::endl;
	cout << SizeCalculation::Index(1024) << std::endl;
}

void TestConcurrentAlloc()
{
	void* ptr0 = hcMalloc(5);
	void* ptr1 = hcMalloc(8);
	void* ptr2 = hcMalloc(8);
	void* ptr3 = hcMalloc(8);

	hcFree(ptr1);
	hcFree(ptr2);
	hcFree(ptr3);
}

//申请大块内存
void TestBigMemory()
{
	void* ptr1 = hcMalloc(65 * 1024);
	hcFree(ptr1);

	void* ptr2 = hcMalloc(129 * 4 * 1024);
	hcFree(ptr2);
}

void TestMallocFree()
{
	/*for (int i = 0; i < 100; i++)
	{
		void* ptr = hcMalloc(8);
		cout<<ptr<<std::endl;
		if (i % 5 == 0)
			hcFree(ptr);
	}*/

	for (int i = 0; i <= 30000; i++)
	{
		srand(i);
		int size = rand();
		size /= 10;
		void* ptr = hcMalloc(size);

			cout << i <<" "<<size<< "  " << ptr << std::endl;
			hcFree(ptr);
	}
}

void testA()
{
	std::vector<void*> ptr;

	for (int i = 1; i <= 50000; i++)
	{
		void* p = hcMalloc(15);
		ptr.push_back(p);
	}

	for (int i = 0; i < 50000; i++)
	{
		hcFree(ptr[i]);
	}

	/*cout << ptr[49999] << std::endl;
	hcFree(ptr[32422]);*/
}
