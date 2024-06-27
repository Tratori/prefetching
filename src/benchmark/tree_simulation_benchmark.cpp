#include "prefetching.hpp"

#include <random>
#include <functional>
#include <chrono>
#include <assert.h>
#include <thread>
#include <iostream>
#include <fstream>
#include <xmmintrin.h>

#include <nlohmann/json.hpp>

#include "numa/numa_memory_resource.hpp"
#include "numa/interleaving_numa_memory_resource.hpp"
#include "utils/utils.cpp"
#include "coroutine.hpp"

unsigned find_in_node(uint32_t *node, uint32_t k, uint32_t values_per_node)
{
    unsigned lower = 0;
    unsigned upper = values_per_node;
    do
    {
        unsigned mid = ((upper - lower) / 2) + lower;
        if (k < node[mid])
        {
            upper = mid;
        }
        else if (k > node[mid])
        {
            lower = mid + 1;
        }
        else
        {
            return mid;
        }
    } while (lower < upper);
    throw std::runtime_error("could not find value in node " + std::to_string(k));
    return values_per_node;
}

task co_find_in_node(uint32_t *node, uint32_t k, uint32_t values_per_node, uint32_t &found_value)
{
    unsigned lower = 0;
    unsigned upper = values_per_node;
    do
    {
        unsigned mid = ((upper - lower) / 2) + lower;
        __builtin_prefetch(reinterpret_cast<void *>(node + mid), 0, 3);
        co_await std::suspend_always{};
        if (k < node[mid])
        {
            upper = mid;
        }
        else if (k > node[mid])
        {
            lower = mid + 1;
        }
        else
        {
            found_value = mid;
            co_return;
        }
    } while (lower < upper);
    throw std::runtime_error("could not find value in node " + std::to_string(k));
    co_return;
}

task co_tree_traversal(size_t num_node_traversal_per_lookup, char *data, uint32_t k,
                       uint32_t values_per_node, size_t tree_node_size,
                       std::uniform_int_distribution<> node_distribution, auto gen)
{
    int sum = 0;
    for (int j = 0; j < num_node_traversal_per_lookup; j++)
    {
        auto next_node = node_distribution(gen);
        uint32_t found;
        co_await co_find_in_node(reinterpret_cast<uint32_t *>(data + (next_node * tree_node_size)), k, values_per_node, found);

        sum += found;
    }
    if (sum != num_node_traversal_per_lookup * k)
    {
        "lookups failed " + std::to_string(sum) + " vs. " + std::to_string(num_node_traversal_per_lookup * k);
    }
}

void tree_simulation_coroutine(size_t repetitions, size_t group_size, size_t values_per_node,
                               size_t num_tree_nodes, size_t tree_node_size,
                               size_t num_node_traversal_per_lookup, char *data)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> uniform_dis_node_value(0, values_per_node - 1);
    std::uniform_int_distribution<> uniform_dis_next_node(0, num_tree_nodes - 1);
    CircularBuffer<task> buff(group_size);

    size_t num_finished = 0;
    int num_scheduled = 0;

    while (num_finished < repetitions)
    {
        task &handle = buff.next_state();
        if (handle.empty || handle.coro.done())
        {
            if (num_scheduled < repetitions)
            {
                auto k = uniform_dis_node_value(gen);
                handle = co_tree_traversal(num_node_traversal_per_lookup, data, k, values_per_node, tree_node_size,
                                           uniform_dis_next_node, gen);
                num_scheduled++;
            }
            if (num_scheduled > group_size)
            {
                num_finished++;
            }
            continue;
        }

        handle.coro.resume();
    }
}

void benchmark_tree_simulation(size_t tree_node_size, NodeID numa_nodes,
                               size_t memory_per_node, size_t num_threads,
                               size_t coroutines, size_t num_lookups,
                               size_t num_node_traversal_per_lookup)
{

    InterleavingNumaMemoryResource mem_res{numa_nodes};
    auto total_memory = memory_per_node * 1024 * 1024 * numa_nodes; // memory given in MiB
    std::pmr::vector<char> data(total_memory, &mem_res);

    auto num_tree_nodes = total_memory / tree_node_size;
    auto values_per_node = tree_node_size / sizeof(u_int32_t); // we use 4B "keys"
    for (size_t i = 0; i < num_tree_nodes; i++)
    {
        for (uint32_t j = 0; j < values_per_node; ++j)
        {
            *(reinterpret_cast<uint32_t *>(data.data() + (tree_node_size * i)) + j) = j;
        }
    }

    auto start = std::chrono::high_resolution_clock::now();
    std::vector<std::jthread> threads;
    for (size_t t = 0; t < num_threads; ++t)
    {
        threads.emplace_back([&]()
                             {
                                  std::random_device rd;
                                  std::mt19937 gen(rd());
                                  std::uniform_int_distribution<> uniform_dis_node_value(0, values_per_node-1);
                                  std::uniform_int_distribution<> uniform_dis_next_node(0, num_tree_nodes-1);
                                  for (size_t i = 0; i < num_lookups / num_threads; ++i){
                                      int next_node;
                                      int searched_value = uniform_dis_node_value(gen);
                                      auto test_counter = 0;
                                      for(size_t j= 0; j < num_node_traversal_per_lookup; ++j){
                                          next_node =  uniform_dis_next_node(gen);
                                          test_counter += find_in_node(reinterpret_cast<uint32_t*>(data.data() + (tree_node_size * next_node)), searched_value, values_per_node);
                                      }
                                      if(test_counter != num_node_traversal_per_lookup * searched_value){
                                          throw std::runtime_error("lookups failed " + std::to_string(test_counter) + " vs. " + std::to_string(num_node_traversal_per_lookup * searched_value));
                                      }
                                  } });
    }
    for (size_t t = 0; t < num_threads; ++t)
    {
        threads[t].join();
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "multithreaded lookup took: " << std::chrono::duration<double>(end - start).count() << " seconds" << std::endl;

    start = std::chrono::high_resolution_clock::now();
    threads.clear();
    for (size_t t = 0; t < num_threads; ++t)
    {
        threads.emplace_back(tree_simulation_coroutine, num_lookups / num_threads, coroutines, values_per_node,
                             num_tree_nodes, tree_node_size, num_node_traversal_per_lookup, data.data());
    }
    for (size_t t = 0; t < num_threads; ++t)
    {
        threads[t].join();
    }
    end = std::chrono::high_resolution_clock::now();
    std::cout << "multithreaded coroutine lookup took: " << std::chrono::duration<double>(end - start).count() << " seconds" << std::endl;
}

int main(int argc, char **argv)
{
    auto &benchmark_config = Prefetching::get().runtime_config;

    // clang-format off
    benchmark_config.add_options()
        ("tree_node_size", "Tree Node size in Bytes", cxxopts::value<std::vector<size_t>>()->default_value("512"))
        ("numa_nodes", "Number of numa nodes to run on", cxxopts::value<std::vector<NodeID>>()->default_value("2"))
        ("memory_per_node", "Memory allocated per numa node in MiB", cxxopts::value<std::vector<size_t>>()->default_value("2048"))
        ("num_threads", "Number of num_threads", cxxopts::value<std::vector<size_t>>()->default_value("8"))
        ("num_lookups", "Number of lookups", cxxopts::value<std::vector<size_t>>()->default_value("10000000"))
        ("num_node_traversal_per_lookup", "Number of distinct node traversals per lookup", cxxopts::value<std::vector<size_t>>()->default_value("10"))
        ("coroutines", "Number of coroutines per thread", cxxopts::value<std::vector<size_t>>()->default_value("20"));
    // clang-format on
    benchmark_config.parse(argc, argv);

    int benchmark_run = 0;
    for (auto &runtime_config : benchmark_config.get_runtime_configs())
    {
        auto tree_node_size = convert<size_t>(runtime_config["tree_node_size"]);
        auto numa_nodes = convert<NodeID>(runtime_config["numa_nodes"]);
        auto memory_per_node = convert<size_t>(runtime_config["memory_per_node"]);
        auto num_threads = convert<size_t>(runtime_config["num_threads"]);
        auto coroutines = convert<size_t>(runtime_config["coroutines"]);
        auto num_lookups = convert<size_t>(runtime_config["num_lookups"]);
        auto num_node_traversal_per_lookup = convert<size_t>(runtime_config["num_node_traversal_per_lookup"]);

        nlohmann::json results;

        benchmark_tree_simulation(tree_node_size, numa_nodes, memory_per_node, num_threads, coroutines, num_lookups, num_node_traversal_per_lookup);
        auto results_file = std::ofstream{"tree_simulation_" + std::to_string(benchmark_run++) + ".json"};
        results_file << results.dump(-1) << std::flush;
    }

    return 0;
}