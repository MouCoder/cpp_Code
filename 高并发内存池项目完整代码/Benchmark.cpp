#include"MemoryManagment.h"

void BenchmarkMalloc(size_t ntimes, size_t nworks, size_t rounds)
{
	//创建nworks个线程
	std::vector<std::thread> vthread(nworks);
	size_t malloc_costtime = 0;
	size_t free_costtime = 0;

	//每个线程循环依次
	for (size_t k = 0; k < nworks; ++k)
	{
		//铺货k
		vthread[k] = std::thread([&, k]() {
			std::vector<void*> v;
			v.reserve(ntimes);

			//执行rounds轮次
			for (size_t j = 0; j < rounds; ++j)
			{
				size_t begin1 = clock();
				//每轮次执行ntimes次
				for (size_t i = 0; i < ntimes; i++)
				{
					v.push_back(malloc(16));
				}
				size_t end1 = clock();

				size_t begin2 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					free(v[i]);
				}
				size_t end2 = clock();
				v.clear();

				malloc_costtime += end1 - begin1;
				free_costtime += end2 - begin2;
			}
		});
	}

	for (auto& t : vthread)
	{
		t.join();
	}

	printf("%u个线程并发执行%u轮次，每轮次malloc %u次: 花费：%u ms\n",
		nworks, rounds, ntimes, malloc_costtime);

	printf("%u个线程并发执行%u轮次，每轮次free %u次: 花费：%u ms\n",
		nworks, rounds, ntimes, free_costtime);

	printf("%u个线程并发malloc&free %u次，总计花费：%u ms\n",
		nworks, nworks*rounds*ntimes, malloc_costtime + free_costtime);
}


// 单轮次申请释放次数 线程数 轮次
void BenchmarkConcurrentMalloc(size_t ntimes, size_t nworks, size_t rounds)
{
	std::vector<std::thread> vthread(nworks);
	size_t malloc_costtime = 0;
	size_t free_costtime = 0;

	for (size_t k = 0; k < nworks; ++k)
	{
		vthread[k] = std::thread([&]() {
			std::vector<void*> v;
			v.reserve(ntimes);

			for (size_t j = 0; j < rounds; ++j)
			{
				size_t begin1 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					v.push_back(hcAlloc(16));
				}
				size_t end1 = clock();

				size_t begin2 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					hcFree(v[i]);
				}
				size_t end2 = clock();
				v.clear();

				malloc_costtime += end1 - begin1;
				free_costtime += end2 - begin2;
			}
		});
	}

	for (auto& t : vthread)
	{
		t.join();
	}

	printf("%u个线程并发执行%u轮次，每轮次concurrent alloc %u次: 花费：%u ms\n",
		nworks, rounds, ntimes, malloc_costtime);

	printf("%u个线程并发执行%u轮次，每轮次concurrent dealloc %u次: 花费：%u ms\n",
		nworks, rounds, ntimes, free_costtime);

	printf("%u个线程并发concurrent alloc&dealloc %u次，总计花费：%u ms\n",
		nworks, nworks*rounds*ntimes, malloc_costtime + free_costtime);
}

int main()
{
	cout << "==========================================================" << endl;
	BenchmarkMalloc(100000, 4, 10);
	cout << endl << endl;

	BenchmarkConcurrentMalloc(100000, 4, 10);
	cout << "==========================================================" << endl;

	return 0;
}