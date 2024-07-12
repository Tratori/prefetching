#include "prefetching.hpp"

#include <random>
#include <x86intrin.h>
#include <chrono>
#include <thread>
#include <iostream>
#include <fstream>
#include <xmmintrin.h>

#include <nlohmann/json.hpp>

#include "numa/numa_memory_resource.hpp"
#include "numa/static_numa_memory_resource.hpp"
#include "utils/utils.cpp"

const size_t CACHELINE_SIZE = get_cache_line_size();
const size_t ACTUAL_PAGE_SIZE = get_page_size();
const auto CLOCK_MIN_DURATION = get_steady_clock_min_duration(1'000'000);

struct PCBenchmarkConfig
{
    size_t total_memory;
    size_t num_threads;
    size_t num_resolves;
    size_t num_parallel_pc;
    size_t accessed_cache_lines;
    bool use_explicit_huge_pages;
    bool madvise_huge_pages;
};

void pointer_chase(size_t thread_id, PCBenchmarkConfig &config, auto &data, auto &durations)
{
    pin_to_cpu(Prefetching::get().numa_manager.node_to_cpus[0][thread_id]);
    uint8_t dependency = 0;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> uniform_dis(0, data.size() - 1);

    std::vector<char *> curr_pointers;
    for (int i = 0; i < config.num_parallel_pc; ++i)
    {
        auto random_pointer = reinterpret_cast<size_t>(data.data() + uniform_dis(gen)) & ~(CACHELINE_SIZE - 1);
        if (random_pointer & (ACTUAL_PAGE_SIZE - 1) < CACHELINE_SIZE)
        {
            random_pointer += CACHELINE_SIZE;
        }
        curr_pointers.push_back(reinterpret_cast<char *>(random_pointer));
    }

    std::vector<std::chrono::duration<double>> local_durations(config.num_resolves / config.num_parallel_pc);

    for (size_t r = 0; r < config.num_resolves / config.num_parallel_pc / config.accessed_cache_lines; r++)
    {
        // Trigger TLB misses by accessing start of page.
        volatile size_t sum = 0;
        for (auto random_pointer : curr_pointers)
        {
            auto padding_page_pointer = reinterpret_cast<char *>(reinterpret_cast<size_t>(random_pointer) & ~(ACTUAL_PAGE_SIZE - 1));
            sum = sum + *padding_page_pointer;
        }
        //  ensure(sum == 0, "padding page area contained != 0");
        _mm_lfence();
        // prefetch actual pointers
        asm volatile("" ::: "memory");

        auto start = std::chrono::steady_clock::now();
        asm volatile("" ::: "memory");

        for (auto random_pointer : curr_pointers)
        {
            for (size_t i = 0; i < config.accessed_cache_lines; ++i)
            {
                __builtin_prefetch(reinterpret_cast<void *>(random_pointer + CACHELINE_SIZE * i), 0, 0); // This can actually access wrong addresses, but prefetch should be allowed to do that.
            }
        }
        asm volatile("" ::: "memory");
        auto end = std::chrono::steady_clock::now();
        asm volatile("" ::: "memory");
        local_durations[r] = end - start - CLOCK_MIN_DURATION;
        for (auto &random_pointer : curr_pointers)
        {
            random_pointer = *reinterpret_cast<char **>(random_pointer);
        }
    }
    durations[thread_id] = findMedian(local_durations, local_durations.size());
};

std::pmr::vector<char> *cached_pointer_chase_arr = nullptr;
void lfb_size_benchmark(PCBenchmarkConfig config, nlohmann::json &results)
{
    StaticNumaMemoryResource mem_res{0, config.use_explicit_huge_pages, config.madvise_huge_pages};
    std::random_device rd;
    std::mt19937 gen(rd());

    auto num_bytes = config.total_memory * 1024 * 1024; // memory given in MiB
    if (!(cached_pointer_chase_arr && (cached_pointer_chase_arr->size() == num_bytes)))
    {
        if (cached_pointer_chase_arr)
        {
            delete cached_pointer_chase_arr;
        }
        cached_pointer_chase_arr = new std::pmr::vector<char>(num_bytes, &mem_res);
        initialize_padded_pointer_chase(*cached_pointer_chase_arr, num_bytes, 0, ACTUAL_PAGE_SIZE, CACHELINE_SIZE);
    }
    auto pointer_chase_arr = *cached_pointer_chase_arr;

    std::vector<std::jthread> threads;

    // ---- Warm-up ----
    auto warm_up_config = PCBenchmarkConfig{config};
    warm_up_config.num_parallel_pc = 16;
    warm_up_config.num_resolves = 100'000;
    std::vector<std::chrono::duration<double>> empty_durations(config.num_threads);
    for (size_t i = 0; i < config.num_threads; ++i)
    {
        threads.emplace_back([&, i]()
                             { pointer_chase(i, config, pointer_chase_arr, empty_durations); });
    }
    for (auto &t : threads)
    {
        t.join();
    }
    threads.clear();
    // ---- End Warm-up ----
    std::vector<std::chrono::duration<double>> durations(config.num_threads);
    for (size_t i = 0; i < config.num_threads; ++i)
    {
        threads.emplace_back([&, i]()
                             { pointer_chase(i, config, pointer_chase_arr, durations); });
    }
    for (auto &t : threads)
    {
        t.join();
    }
    auto total_time = std::chrono::duration<double>{0};
    for (auto &duration : durations)
    {
        total_time += duration;
    }
    results["runtime"] = total_time.count();
    std::cout << config.num_parallel_pc << " took " << total_time.count() << std::endl;
}

int main(int argc, char **argv)
{
    auto &benchmark_config = Prefetching::get().runtime_config;

    // clang-format off
    benchmark_config.add_options()
        ("total_memory", "Total memory allocated MiB", cxxopts::value<std::vector<size_t>>()->default_value("1024"))
        ("num_threads", "Number of threads running the bench", cxxopts::value<std::vector<size_t>>()->default_value("1"))
        ("num_resolves", "Number of resolves each pointer chase executes", cxxopts::value<std::vector<size_t>>()->default_value("1000000"))
        ("num_parallel_pc", "Number of parallel pointer chases per thread", cxxopts::value<std::vector<size_t>>()->default_value("10"))
        ("accessed_cache_lines", "Number cache lines that are prefetched per pointer resolve", cxxopts::value<std::vector<size_t>>()->default_value("1"))
        ("use_explicit_huge_pages", "Use huge pages during allocation", cxxopts::value<std::vector<bool>>()->default_value("false"))
        ("madvise_huge_pages", "Madvise kernel to create huge pages on mem regions", cxxopts::value<std::vector<bool>>()->default_value("true"));
    // clang-format on
    benchmark_config.parse(argc, argv);

    int benchmark_run = 0;

    std::vector<nlohmann::json> all_results;
    for (auto &runtime_config : benchmark_config.get_runtime_configs())
    {
        auto total_memory = convert<size_t>(runtime_config["total_memory"]);
        auto num_threads = convert<size_t>(runtime_config["num_threads"]);
        auto num_resolves = convert<size_t>(runtime_config["num_resolves"]);
        auto num_parallel_pc = convert<size_t>(runtime_config["num_parallel_pc"]);
        auto accessed_cache_lines = convert<size_t>(runtime_config["accessed_cache_lines"]);
        auto use_explicit_huge_pages = convert<bool>(runtime_config["use_explicit_huge_pages"]);
        auto madvise_huge_pages = convert<bool>(runtime_config["madvise_huge_pages"]);
        PCBenchmarkConfig config = {
            total_memory,
            num_threads,
            num_resolves,
            num_parallel_pc,
            accessed_cache_lines,
            use_explicit_huge_pages,
            madvise_huge_pages,
        };
        nlohmann::json results;
        results["config"]["total_memory"] = config.total_memory;
        results["config"]["num_threads"] = config.num_threads;
        results["config"]["num_resolves"] = config.num_resolves;
        results["config"]["num_parallel_pc"] = config.num_parallel_pc;
        results["config"]["accessed_cache_lines"] = config.accessed_cache_lines;
        results["config"]["use_explicit_huge_pages"] = config.use_explicit_huge_pages;
        results["config"]["madvise_huge_pages"] = config.madvise_huge_pages;

        lfb_size_benchmark(config, results);
        all_results.push_back(results);
        auto results_file = std::ofstream{"pc_benchmark.json"};
        nlohmann::json intermediate_json;
        intermediate_json["results"] = all_results;
        results_file << intermediate_json.dump(-1) << std::flush;
    }

    return 0;
}
