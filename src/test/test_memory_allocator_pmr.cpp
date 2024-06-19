#include "utils/numa_memory_resource.hpp"
#include <list>
#include <iostream>

int main()
{
    NumaMemoryResource mem_res{};

    std::pmr::vector<int> test(&mem_res);

    for (int i = 0; i < 10000; ++i)
    {
        test.push_back(i);
    }

    std::pmr::vector<std::pmr::list<int>> test2(&mem_res);

    std::cout << "adding 100 lists" << std::endl;
    for (int i = 0; i < 100; ++i)
    {
        test2.emplace_back(std::pmr::list<int>(&mem_res));
    }
    std::cout << "adding 100 elements to first list" << std::endl;
    for (int i = 0; i < 100; ++i)
    {
        test2[0].emplace_back(i);
    }
    return 0;
}