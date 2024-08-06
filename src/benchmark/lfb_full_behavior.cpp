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
const size_t REPEATS = 10;

struct LFBFullBenchmarkConfig
{
    size_t total_memory;
    size_t num_threads;
    size_t num_repetitions;
    size_t num_prefetches;
    size_t measure_until;
    bool use_explicit_huge_pages;
    bool madvise_huge_pages;
};

void generate_stats(auto &results, auto &durations, std::string prefix)
{
    results["min_" + prefix + "runtime"] = *std::min_element(durations.begin(), durations.end());
    results["median_" + prefix + "runtime"] = findMedian(durations, durations.size());
    results[prefix + "runtime"] = results["median_" + prefix + "runtime"];
    results[prefix + "runtimes"] = durations;
}

void prefetch_full(size_t i, size_t number_accesses, auto &config, auto &data, auto &accesses, auto &durations, const bool prefetch = true)
{
    pin_to_cpu(Prefetching::get().numa_manager.node_to_available_cpus[0][i]);
    size_t start_access = i * number_accesses;
    int dummy_dependency = 0; // data has only zeros written to it, so this will effectively do nothing, besides
                              // adding a data dependency - Hopefully this forces batches to be loaded sequentially.

    auto new_dummy_dependency = dummy_dependency;
    std::random_device rd;
    std::chrono::duration<double> duration{0};
    const auto measure_until = (config.measure_until == 0) ? config.num_prefetches / 2 : config.measure_until;
    for (size_t b = 0; b + config.num_prefetches < number_accesses; b += config.num_prefetches)
    {
        if (prefetch)
        {
            for (int j = 0; j < config.num_prefetches; ++j)
            {
                __builtin_prefetch(reinterpret_cast<void *>(data.data() + accesses[start_access + b + j + dummy_dependency]), 0, 0);
            }
        }

        auto start = std::chrono::high_resolution_clock::now();
        for (int j = 0; j < measure_until; ++j)
        {
            auto random_access = accesses[start_access + b + dummy_dependency + j];
            new_dummy_dependency += *reinterpret_cast<uint8_t *>(data.data() + random_access);
        }
        auto end = std::chrono::high_resolution_clock::now();
        duration += end - start;

        for (int j = measure_until; j < config.num_prefetches; ++j)
        {
            auto random_access = accesses[start_access + b + dummy_dependency + j];
            new_dummy_dependency += *reinterpret_cast<uint8_t *>(data.data() + random_access);
        }
        dummy_dependency = new_dummy_dependency;
    }
    if (dummy_dependency > data.size())
    {
        throw std::runtime_error("new_dep contains wrong dependency: " + std::to_string(dummy_dependency));
    }
    durations[i] = duration;
};

void benchmark_wrapper(LFBFullBenchmarkConfig config, nlohmann::json &results, auto &zero_data)
{

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, zero_data.size() - 1);

    std::vector<std::uint64_t> accesses(config.num_repetitions);
    std::vector<double> measurement_lower_durations(REPEATS);
    std::vector<double> measurement_durations(REPEATS);
    std::vector<double> measurement_upper_durations(REPEATS);

    for (size_t r = 0; r < REPEATS; r++)
    {
        size_t number_accesses_per_thread = config.num_repetitions / config.num_threads;
        std::vector<std::jthread> threads;
        std::vector<std::chrono::duration<double>> baseline_durations(config.num_threads);

        // Optimum (all "random accesses" point to the first element) -> every access should be cached
        std::generate(accesses.begin(), accesses.end(), [&]()
                      { return 0; });

        for (size_t i = 0; i < config.num_threads; ++i)
        {
            threads.emplace_back([&, i]()
                                 { prefetch_full(i, number_accesses_per_thread, config, zero_data, accesses, baseline_durations); });
        }
        for (auto &t : threads)
        {
            t.join();
        }
        auto total_baseline_time = std::chrono::duration<double>{0};
        for (auto &duration : baseline_durations)
        {
            total_baseline_time += duration;
        }
        measurement_lower_durations[r] = total_baseline_time.count();
        threads.clear();
        // now actually generate random accesses and prefetch
        std::generate(accesses.begin(), accesses.end(), [&]()
                      { return dis(gen); });

        std::vector<std::chrono::duration<double>> durations(config.num_threads);
        for (size_t i = 0; i < config.num_threads; ++i)
        {
            threads.emplace_back([&, i]()
                                 { prefetch_full(i, number_accesses_per_thread, config, zero_data, accesses, durations); });
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
        measurement_durations[r] = total_time.count();
        // Upper bound -> every measured access should be dram
        threads.clear();
        std::generate(accesses.begin(), accesses.end(), [&]()
                      { return dis(gen); });
        std::vector<std::chrono::duration<double>> upper_durations(config.num_threads);

        for (size_t i = 0; i < config.num_threads; ++i)
        {
            threads.emplace_back([&, i]()
                                 { prefetch_full(i, number_accesses_per_thread, config, zero_data, accesses, upper_durations, false); });
        }
        for (auto &t : threads)
        {
            t.join();
        }
        auto total_upper_time = std::chrono::duration<double>{0};
        for (auto &duration : upper_durations)
        {
            total_upper_time += duration;
        }
        measurement_upper_durations[r] = total_upper_time.count();
    }
    generate_stats(results, measurement_lower_durations, "lower_");
    generate_stats(results, measurement_durations, "");
    generate_stats(results, measurement_upper_durations, "upper_");

    std::cout << "num_prefetches: " << config.num_prefetches << " measure_until: " << config.measure_until << std::endl;
    std::cout << "took " << results["lower_runtime"] << " / " << results["runtime"] << " / " << results["upper_runtime"] << std::endl;
}

int main(int argc, char **argv)
{
    auto &benchmark_config = Prefetching::get().runtime_config;

    // clang-format off
    benchmark_config.add_options()
        ("total_memory", "Total memory allocated MiB", cxxopts::value<std::vector<size_t>>()->default_value("2048"))
        ("num_threads", "Number of threads running the bench", cxxopts::value<std::vector<size_t>>()->default_value("1"))
        ("num_repetitions", "Number of repetitions of the measurement", cxxopts::value<std::vector<size_t>>()->default_value("10000000"))
        ("start_num_prefetches", "starting number of prefetches between corresponding prefetch and load", cxxopts::value<std::vector<size_t>>()->default_value("2"))
        ("end_num_prefetches", "ending number of prefetches between corresponding prefetch and load", cxxopts::value<std::vector<size_t>>()->default_value("256"))
        ("start_measure_until", "start number of first x values for which we measure access time, if start==end==0 we take num_prefetches/2", cxxopts::value<std::vector<size_t>>()->default_value("0"))
        ("end_measure_until", "end number of first x values for which we measure access time, if start==end==0 we take num_prefetches/2", cxxopts::value<std::vector<size_t>>()->default_value("0"))
        ("madvise_huge_pages", "Madvise kernel to create huge pages on mem regions", cxxopts::value<std::vector<bool>>()->default_value("true"))
        ("use_explicit_huge_pages", "Use huge pages during allocation", cxxopts::value<std::vector<bool>>()->default_value("false"))
        ("out", "Path on which results should be stored", cxxopts::value<std::vector<std::string>>()->default_value("lfb_full_behavior.json"));
    // clang-format on
    benchmark_config.parse(argc, argv);

    int benchmark_run = 0;

    std::vector<nlohmann::json> all_results;
    for (auto &runtime_config : benchmark_config.get_runtime_configs())
    {
        auto total_memory = convert<size_t>(runtime_config["total_memory"]);
        auto num_threads = convert<size_t>(runtime_config["num_threads"]);
        auto num_repetitions = convert<size_t>(runtime_config["num_repetitions"]);
        auto start_num_prefetches = convert<size_t>(runtime_config["start_num_prefetches"]);
        auto end_num_prefetches = convert<size_t>(runtime_config["end_num_prefetches"]);
        auto start_measure_until = convert<size_t>(runtime_config["start_measure_until"]);
        auto end_measure_until = convert<size_t>(runtime_config["end_measure_until"]);
        auto madvise_huge_pages = convert<bool>(runtime_config["madvise_huge_pages"]);
        auto use_explicit_huge_pages = convert<bool>(runtime_config["use_explicit_huge_pages"]);
        auto out = convert<std::string>(runtime_config["out"]);

        LFBFullBenchmarkConfig config = {
            total_memory,
            num_threads,
            num_repetitions,
            0,
            0,
            use_explicit_huge_pages,
            madvise_huge_pages,
        };

        auto total_memory_bytes = config.total_memory * 1024 * 1024; // memory given in MiB
        StaticNumaMemoryResource mem_res{0, config.use_explicit_huge_pages, config.madvise_huge_pages};

        std::pmr::vector<char> data(total_memory_bytes, &mem_res);

        memset(data.data(), total_memory_bytes, 0);
        // sleep(total_memory / 1.2);
        for (size_t num_prefetches = start_num_prefetches; num_prefetches <= end_num_prefetches; num_prefetches++)
        {
            for (size_t measure_until = start_measure_until; measure_until <= end_measure_until; measure_until++)
            {
                nlohmann::json results;
                results["config"]["total_memory"] = config.total_memory;
                results["config"]["num_threads"] = config.num_threads;
                results["config"]["num_repetitions"] = config.num_repetitions;
                results["config"]["num_prefetched"] = num_prefetches;
                results["config"]["use_explicit_huge_pages"] = config.use_explicit_huge_pages;
                results["config"]["madvise_huge_pages"] = config.madvise_huge_pages;
                results["config"]["measure_until"] = measure_until;
                config.num_prefetches = num_prefetches;
                config.measure_until = measure_until;
                benchmark_wrapper(config, results, data);
                all_results.push_back(results);
            }
        }
        auto results_file = std::ofstream{out};
        nlohmann::json intermediate_json;
        intermediate_json["results"] = all_results;
        results_file << intermediate_json.dump(-1) << std::flush;
    }

    return 0;
}