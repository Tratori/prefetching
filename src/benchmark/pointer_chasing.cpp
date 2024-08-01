#include "prefetching.hpp"

#include <random>
#include <chrono>
#include <thread>
#include <iostream>
#include <fstream>

#include <nlohmann/json.hpp>

#include "numa/numa_memory_resource.hpp"
#include "numa/static_numa_memory_resource.hpp"
#include "utils/utils.cpp"

const size_t CACHELINE_SIZE = get_cache_line_size();
const size_t ACTUAL_PAGE_SIZE = get_page_size();

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
    const auto &numa_manager = Prefetching::get().numa_manager;
    pin_to_cpu(numa_manager.node_to_available_cpus[numa_manager.active_nodes[0]][thread_id]);
    uint8_t dependency = 0;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> uniform_dis(0, data.size() - 1);

    std::vector<uint64_t> curr_pointers;
    for (int i = 0; i < config.num_parallel_pc; ++i)
    {
        curr_pointers.push_back(uniform_dis(gen));
    }

    const auto number_repetitions = config.num_resolves / config.num_parallel_pc / config.accessed_cache_lines;
    std::vector<unsigned long long> local_durations(number_repetitions);

    for (size_t r = 0; r < number_repetitions; r++)
    {
        lfence();
        // prefetch actual pointers
        asm volatile("" ::: "memory");

        auto start = read_cycles();
        asm volatile("" ::: "memory");

        for (auto random_pointer : curr_pointers)
        {
            for (size_t i = 0; i < config.accessed_cache_lines; ++i)
            {
                __builtin_prefetch(reinterpret_cast<void *>(data.data() + random_pointer + CACHELINE_SIZE * i), 0, 0); // This can actually access wrong addresses, but prefetch should be allowed to do that.
            }
        }
        asm volatile("" ::: "memory");
        auto end = read_cycles();
        asm volatile("" ::: "memory");
        local_durations[r] = end - start;

        // Proceed to the next pointer and also perform some work to make cpu reordering harder.
        volatile uint32_t work_sum = 0;
        const size_t WORK_FACTOR = 8;
        for (auto &random_pointer : curr_pointers)
        {
            random_pointer = *(data.data() + random_pointer);
            for (size_t w = 0; w < WORK_FACTOR; ++w)
            {
                work_sum = work_sum + murmur_32(*(data.data() + random_pointer) >> w);
            }
        }
    }
    durations[thread_id] = findMedian(local_durations, local_durations.size());
};

void lfb_size_benchmark(PCBenchmarkConfig config, nlohmann::json &results, auto &pointer_chase_arr)
{
    StaticNumaMemoryResource mem_res{Prefetching::get().numa_manager.active_nodes[0], config.use_explicit_huge_pages, config.madvise_huge_pages};
    std::random_device rd;
    std::mt19937 gen(rd());

    std::vector<std::jthread> threads;

    // ---- Warm-up ----
    auto warm_up_config = PCBenchmarkConfig{config};
    warm_up_config.num_parallel_pc = 16;
    warm_up_config.num_resolves = 100'000;
    std::vector<unsigned long long> empty_durations(config.num_threads);
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
    std::vector<unsigned long long> durations(config.num_threads);
    for (size_t i = 0; i < config.num_threads; ++i)
    {
        threads.emplace_back([&, i]()
                             { pointer_chase(i, config, pointer_chase_arr, durations); });
    }
    for (auto &t : threads)
    {
        t.join();
    }
    unsigned long long total_time = 0;
    for (auto &duration : durations)
    {
        total_time += duration;
    }
    results["runtime"] = total_time;
    std::cout << config.num_parallel_pc << " took " << total_time << std::endl;
}

int main(int argc, char **argv)
{
    auto &benchmark_config = Prefetching::get().runtime_config;

    // clang-format off
    benchmark_config.add_options()
        ("total_memory", "Total memory allocated MiB", cxxopts::value<std::vector<size_t>>()->default_value("1024"))
        ("num_threads", "Number of threads running the bench", cxxopts::value<std::vector<size_t>>()->default_value("1"))
        ("num_resolves", "Number of resolves each pointer chase executes", cxxopts::value<std::vector<size_t>>()->default_value("1000000"))
        ("start_num_parallel_pc", "Start number of parallel pointer chases per thread", cxxopts::value<std::vector<size_t>>()->default_value("1"))
        ("end_num_parallel_pc", "End number of parallel pointer chases per thread", cxxopts::value<std::vector<size_t>>()->default_value("128"))
        ("accessed_cache_lines", "Number cache lines that are prefetched per pointer resolve", cxxopts::value<std::vector<size_t>>()->default_value("1"))
        ("use_explicit_huge_pages", "Use huge pages during allocation", cxxopts::value<std::vector<bool>>()->default_value("false"))
        ("madvise_huge_pages", "Madvise kernel to create huge pages on mem regions", cxxopts::value<std::vector<bool>>()->default_value("true"))
        ("out", "Path on which results should be stored", cxxopts::value<std::vector<std::string>>()->default_value("pc_benchmark.json"));
    // clang-format on
    benchmark_config.parse(argc, argv);

    int benchmark_run = 0;

    std::vector<nlohmann::json> all_results;
    for (auto &runtime_config : benchmark_config.get_runtime_configs())
    {
        auto total_memory = convert<size_t>(runtime_config["total_memory"]);
        auto num_threads = convert<size_t>(runtime_config["num_threads"]);
        auto num_resolves = convert<size_t>(runtime_config["num_resolves"]);
        auto start_num_parallel_pc = convert<size_t>(runtime_config["start_num_parallel_pc"]);
        auto end_num_parallel_pc = convert<size_t>(runtime_config["end_num_parallel_pc"]);
        auto accessed_cache_lines = convert<size_t>(runtime_config["accessed_cache_lines"]);
        auto use_explicit_huge_pages = convert<bool>(runtime_config["use_explicit_huge_pages"]);
        auto madvise_huge_pages = convert<bool>(runtime_config["madvise_huge_pages"]);
        auto out = convert<std::string>(runtime_config["out"]);

        PCBenchmarkConfig config = {
            total_memory,
            num_threads,
            num_resolves,
            start_num_parallel_pc,
            accessed_cache_lines,
            use_explicit_huge_pages,
            madvise_huge_pages,
        };

        StaticNumaMemoryResource mem_res{Prefetching::get().numa_manager.active_nodes[0], config.use_explicit_huge_pages, config.madvise_huge_pages};

        auto num_bytes = config.total_memory * 1024 * 1024; // memory given in MiB

        std::pmr::vector<uint64_t> pc_array(num_bytes / sizeof(uint64_t), &mem_res);
        initialize_pointer_chase(pc_array.data(), pc_array.size());

        for (size_t num_parallel_pc = start_num_parallel_pc; num_parallel_pc <= end_num_parallel_pc; num_parallel_pc++)
        {
            nlohmann::json results;
            results["config"]["total_memory"] = config.total_memory;
            results["config"]["num_threads"] = config.num_threads;
            results["config"]["num_resolves"] = config.num_resolves;
            results["config"]["num_parallel_pc"] = config.num_parallel_pc;
            results["config"]["accessed_cache_lines"] = config.accessed_cache_lines;
            results["config"]["use_explicit_huge_pages"] = config.use_explicit_huge_pages;
            results["config"]["madvise_huge_pages"] = config.madvise_huge_pages;

            config.num_parallel_pc = num_parallel_pc;
            lfb_size_benchmark(config, results, pc_array);
            all_results.push_back(results);
            auto results_file = std::ofstream{out};
            nlohmann::json intermediate_json;
            intermediate_json["results"] = all_results;
            results_file << intermediate_json.dump(-1) << std::flush;
        }
    }

    return 0;
}
