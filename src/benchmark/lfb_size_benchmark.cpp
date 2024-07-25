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

struct LFBBenchmarkConfig
{
    size_t total_memory;
    size_t num_threads;
    size_t num_repetitions;
    size_t batch_size;
    size_t prefetch_distance;
    size_t resolve_cachelines;
    bool use_explicit_huge_pages;
    bool madvise_huge_pages;
};

void batched_load(size_t i, size_t number_accesses, auto &config, auto &data, auto &accesses, auto &durations)
{
    pin_to_cpu(Prefetching::get().numa_manager.node_to_cpus[0][i]);
    size_t start_access = i * number_accesses;
    int dummy_dependency = 0; // data has only zeros written to it, so this will effectively do nothing, besides
                              // adding a data dependency - Hopefully this forces batches to be loaded sequentially.

    const auto num_batches = number_accesses / config.batch_size;
    const auto data_size = data.size();
    auto start = std::chrono::high_resolution_clock::now();
    for (size_t b = 0; b < num_batches; ++b)
    {
        auto offset = start_access + (b * config.batch_size); // thread offset
        for (size_t i = 0; i < config.batch_size; ++i)
        {
            auto random_access = accesses[offset + config.prefetch_distance * config.batch_size + i] + dummy_dependency;
            for (size_t j = 0; j < config.resolve_cachelines; j++)
            {
                __builtin_prefetch(reinterpret_cast<void *>(data.data() + random_access + CACHELINE_SIZE * j), 0, 0);
            }
        }
        for (size_t i = 0; i < config.batch_size; ++i)
        {
            auto random_access = accesses[offset + i] + dummy_dependency;
            for (size_t j = 0; j < config.resolve_cachelines; j++)
            {
                dummy_dependency += *reinterpret_cast<uint8_t *>(data.data() + random_access + CACHELINE_SIZE * j);
            }
        }
        // LFB CLEAR
        // idle computation
        // noops
    }
    if (dummy_dependency > data_size)
    {
        throw std::runtime_error("new_dep contains wrong dependency: " + std::to_string(dummy_dependency));
    }
    auto end = std::chrono::high_resolution_clock::now();
    durations[i] = std::chrono::duration<double>(end - start);
};

void batched_load_simplified(size_t i, size_t number_accesses, auto &config, auto &data, auto &accesses, auto &durations)
{
    pin_to_cpu(Prefetching::get().numa_manager.node_to_cpus[0][i]);
    size_t start_access = i * number_accesses;
    int dummy_dependency = 0; // data has only zeros written to it, so this will effectively do nothing, besides
                              // adding a data dependency - Hopefully this forces batches to be loaded sequentially.

    const auto data_size = data.size();
    auto start = std::chrono::high_resolution_clock::now();
    for (size_t b = 0; b < number_accesses; ++b)
    {
        auto offset = start_access + b; // thread offset
        auto random_access = accesses[offset + config.prefetch_distance] + dummy_dependency;
        __builtin_prefetch(reinterpret_cast<void *>(data.data() + random_access), 0, 0);
        random_access = accesses[offset] + dummy_dependency;
        dummy_dependency += *reinterpret_cast<uint8_t *>(data.data() + random_access);
    }
    if (dummy_dependency > data_size)
    {
        throw std::runtime_error("new_dep contains wrong dependency: " + std::to_string(dummy_dependency));
    }
    auto end = std::chrono::high_resolution_clock::now();
    durations[i] = std::chrono::duration<double>(end - start);
};

void lfb_size_benchmark(LFBBenchmarkConfig config, nlohmann::json &results, auto &zero_data)
{

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, zero_data.size() - 1 - config.resolve_cachelines * CACHELINE_SIZE);

    std::vector<std::uint64_t> accesses(config.num_repetitions);

    const size_t read_size = 8;
    // fill accesses with random numbers from 0 to total_memory (in bytes) - read_size.
    std::generate(accesses.begin(), accesses.end(), [&]()
                  { return dis(gen); });

    std::vector<std::jthread> threads;

    std::vector<std::chrono::duration<double>> durations(config.num_threads);
    size_t number_accesses_per_thread = config.num_repetitions / config.num_threads;
    for (size_t i = 0; i < config.num_threads; ++i)
    {
        threads.emplace_back([&, i]()
                             { batched_load_simplified(i, number_accesses_per_thread, config, zero_data, accesses, durations); });
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
    std::cout << "batch_size: " << config.batch_size << " prefetch_distance: " << config.prefetch_distance << " resolve_cache_lines " << config.resolve_cachelines << std::endl;
    std::cout << "took " << total_time.count() << std::endl;
}

int main(int argc, char **argv)
{
    auto &benchmark_config = Prefetching::get().runtime_config;

    // clang-format off
    benchmark_config.add_options()
        ("total_memory", "Total memory allocated MiB", cxxopts::value<std::vector<size_t>>()->default_value("1024"))
        ("num_threads", "Number of threads running the bench", cxxopts::value<std::vector<size_t>>()->default_value("8"))
        ("num_repetitions", "Number of repetitions of the measurement", cxxopts::value<std::vector<size_t>>()->default_value("10000000"))
        ("batch_size", "Number of distinct prefetch instructions before loads start", cxxopts::value<std::vector<size_t>>()->default_value("10"))
        ("start_prefetch_distance", "starting number of prefetches between corresponding prefetch and load", cxxopts::value<std::vector<size_t>>()->default_value("0"))
        ("end_prefetch_distance", "ending number of prefetches between corresponding prefetch and load", cxxopts::value<std::vector<size_t>>()->default_value("64"))
        ("resolve_cachelines", "number of cachelines to load per random resolve", cxxopts::value<std::vector<size_t>>()->default_value("1"))
        ("madvise_huge_pages", "Madvise kernel to create huge pages on mem regions", cxxopts::value<std::vector<bool>>()->default_value("true"))
        ("use_explicit_huge_pages", "Use huge pages during allocation", cxxopts::value<std::vector<bool>>()->default_value("false"))
        ("out", "Path on which results should be stored", cxxopts::value<std::vector<std::string>>()->default_value("lfb_size.json"));
    // clang-format on
    benchmark_config.parse(argc, argv);

    int benchmark_run = 0;

    std::vector<nlohmann::json> all_results;
    for (auto &runtime_config : benchmark_config.get_runtime_configs())
    {
        auto total_memory = convert<size_t>(runtime_config["total_memory"]);
        auto num_threads = convert<size_t>(runtime_config["num_threads"]);
        auto num_repetitions = convert<size_t>(runtime_config["num_repetitions"]);
        auto batch_size = convert<size_t>(runtime_config["batch_size"]);
        auto start_prefetching_distance = convert<size_t>(runtime_config["start_prefetch_distance"]);
        auto end_prefetching_distance = convert<size_t>(runtime_config["end_prefetch_distance"]);
        auto resolve_cachelines = convert<size_t>(runtime_config["resolve_cachelines"]);
        auto madvise_huge_pages = convert<bool>(runtime_config["madvise_huge_pages"]);
        auto use_explicit_huge_pages = convert<bool>(runtime_config["use_explicit_huge_pages"]);
        auto out = convert<std::string>(runtime_config["out"]);

        LFBBenchmarkConfig config = {
            total_memory,
            num_threads,
            num_repetitions,
            batch_size,
            0,
            resolve_cachelines,
            use_explicit_huge_pages,
            madvise_huge_pages,
        };

        auto total_memory_bytes = config.total_memory * 1024 * 1024; // memory given in MiB
        StaticNumaMemoryResource mem_res{0, config.use_explicit_huge_pages, config.madvise_huge_pages};

        std::pmr::vector<char> data(total_memory_bytes, &mem_res);

        memset(data.data(), total_memory_bytes, 0);
        sleep(total_memory / 1.2);
        for (size_t prefetching_distance = start_prefetching_distance; prefetching_distance <= end_prefetching_distance; prefetching_distance++)
        {
            nlohmann::json results;
            results["config"]["total_memory"] = config.total_memory;
            results["config"]["num_threads"] = config.num_threads;
            results["config"]["num_repetitions"] = config.num_repetitions;
            results["config"]["batch_size"] = config.batch_size;
            results["config"]["prefetch_distance"] = prefetching_distance;
            results["config"]["resolve_cachelines"] = config.resolve_cachelines;
            results["config"]["use_explicit_huge_pages"] = config.use_explicit_huge_pages;
            results["config"]["madvise_huge_pages"] = config.madvise_huge_pages;
            config.prefetch_distance = prefetching_distance;
            lfb_size_benchmark(config, results, data);
            all_results.push_back(results);
        }
        auto results_file = std::ofstream{out};
        nlohmann::json intermediate_json;
        intermediate_json["results"] = all_results;
        results_file << intermediate_json.dump(-1) << std::flush;
    }

    return 0;
}