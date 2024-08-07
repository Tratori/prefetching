#include "prefetching.hpp"

#include <algorithm>
#include <random>
#include <chrono>
#include <thread>
#include <iostream>
#include <fstream>

#include <nlohmann/json.hpp>

#include "numa/numa_memory_resource.hpp"
#include "numa/static_numa_memory_resource.hpp"
#include "utils/utils.cpp"
#include "../../third_party/tinymembench/tinymembench.h"
#include "../../third_party/tinymembench/util.h"

struct LBenchmarkConfig
{
    int memory_size;
    int access_range;
    NodeID alloc_on_node;
    NodeID run_on_node;
    int repeats;
    bool use_explicit_huge_pages;
    bool madvise_huge_pages;
};

// Adapted from Tinymembench
int latency_bench(LBenchmarkConfig &config, auto &results, char *buffer)
{
    double t, t2, t_before, t_after, t_noaccess, t_noaccess2;
    double xs, xs0, xs1, xs2;
    double ys, ys0, ys1, ys2;
    double min_t, min_t2;
    int nbits, n;
    pin_to_cpu(Prefetching::get().numa_manager.node_to_available_cpus[config.run_on_node][0]);

    for (n = 1; n <= MAXREPEATS; n++)
    {
        t_before = gettime();
        random_read_test(buffer, config.repeats, 1);
        t_after = gettime();
        if (n == 1 || t_after - t_before < t_noaccess)
            t_noaccess = t_after - t_before;

        t_before = gettime();
        random_dual_read_test(buffer, config.repeats, 1);
        t_after = gettime();
        if (n == 1 || t_after - t_before < t_noaccess2)
            t_noaccess2 = t_after - t_before;
    }

    printf("\nblock size : single random read / dual random read");
    if (!config.madvise_huge_pages && !config.use_explicit_huge_pages)
        printf(", [NOHUGEPAGE]\n");
    else if (config.madvise_huge_pages)
        printf(", [MADV_HUGEPAGE]\n");
    else if (config.use_explicit_huge_pages)
        printf(", [MMAP_HUGEPAGE]\n");
    else
        throw std::logic_error("hugepage config wrong");

    int testsize = config.access_range;
    xs1 = xs2 = ys = ys1 = ys2 = 0;
    for (n = 1; n <= MAXREPEATS; n++)
    {
        /*
         * Select a random offset in order to mitigate the unpredictability
         * of cache associativity effects when dealing with different
         * physical memory fragmentation (for PIPT caches). We are reporting
         * the "best" measured latency, some offsets may be better than
         * the others.
         */
        int testoffs = (rand32() % ((config.memory_size << 20) / testsize)) * testsize;

        t_before = gettime();
        random_read_test(buffer + testoffs, config.repeats, testsize);
        t_after = gettime();
        t = t_after - t_before - t_noaccess;
        if (t < 0)
            t = 0;

        xs1 += t;
        xs2 += t * t;

        if (n == 1 || t < min_t)
            min_t = t;

        t_before = gettime();
        random_dual_read_test(buffer + testoffs, config.repeats, testsize);
        t_after = gettime();
        t2 = t_after - t_before - t_noaccess2;
        if (t2 < 0)
            t2 = 0;

        ys1 += t2;
        ys2 += t2 * t2;

        if (n == 1 || t2 < min_t2)
            min_t2 = t2;

        if (n > 2)
        {
            xs = sqrt((xs2 * n - xs1 * xs1) / (n * (n - 1)));
            ys = sqrt((ys2 * n - ys1 * ys1) / (n * (n - 1)));
            if (xs < min_t / 1000. && ys < min_t2 / 1000.)
                break;
        }
    }
    printf("%10d : %6.1f ns          /  %6.1f ns \n", config.access_range,
           min_t * 1000000000. / config.repeats, min_t2 * 1000000000. / config.repeats);

    results["latency_single"] = min_t * 1000000000. / config.repeats;
    results["latency_double"] = min_t2 * 1000000000. / config.repeats;
    return 1;
}

size_t resolve(volatile size_t *buffer, size_t resolves, size_t start)
{
    size_t count = 0;
    size_t curr = start;
    while (count < resolves)
    {
        curr = buffer[curr];
        count++;
    }
    return curr;
}

int pointer_chase(LBenchmarkConfig &config, auto &results, size_t *buffer)
{
    sleep(5);
    size_t *zero_buffer;
    pin_to_cpu(Prefetching::get().numa_manager.node_to_available_cpus[config.run_on_node][0]);
    auto memRes = StaticNumaMemoryResource(config.alloc_on_node, config.use_explicit_huge_pages, config.madvise_huge_pages);

    initialize_pointer_chase(buffer, config.access_range / sizeof(size_t));

    zero_buffer = reinterpret_cast<size_t *>(memRes.allocate(sizeof(size_t), 1 << 22));
    zero_buffer[0] = 0;

    std::cout << std::endl
              << "block size : single random read";
    if (!config.madvise_huge_pages && !config.use_explicit_huge_pages)
        std::cout << ", [NOHUGEPAGE]\n";
    else if (config.madvise_huge_pages)
        std::cout << ", [MADV_HUGEPAGE]\n";
    else if (config.use_explicit_huge_pages)
        std::cout << ", [MMAP_HUGEPAGE]\n";
    else
        throw std::logic_error("hugepage config wrong");
    std::vector<std::chrono::duration<double>> baseline_durations(MAXREPEATS);
    std::vector<std::chrono::duration<double>> access_durations(MAXREPEATS);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, (config.access_range / sizeof(size_t)) - 1);

    for (int n = 0; n < MAXREPEATS; n++)
    {
        auto start = std::chrono::high_resolution_clock::now();
        auto r = resolve(zero_buffer, config.repeats, 0);
        auto end = std::chrono::high_resolution_clock::now();

        baseline_durations[n] = end - start;

        start = std::chrono::high_resolution_clock::now();
        r += resolve(buffer, config.repeats, dis(gen));
        end = std::chrono::high_resolution_clock::now();

        access_durations[n] = end - start;

        if (r > config.access_range / sizeof(size_t))
        {
            throw std::runtime_error("error occurred during resolve. " + std::to_string(r) + " returned.");
        }
    }
    std::cout << "Baseline_Durations:";
    for (auto &no_d : baseline_durations)
    {
        std::cout << " " << no_d.count();
    }
    std::cout << std::endl;
    std::cout << "Access_durations:";
    for (auto &d : access_durations)
    {
        std::cout << " " << d.count();
    }
    std::cout << std::endl;
    results["min_latency_single"] = ((*std::min_element(access_durations.begin(), access_durations.end()) - *std::min_element(baseline_durations.begin(), baseline_durations.end())) / (double)config.repeats * 1'000'000'000).count();
    results["median_latency_single"] = ((findMedian(access_durations, access_durations.size()) - findMedian(baseline_durations, baseline_durations.size())) / (double)config.repeats * 1'000'000'000).count();
    results["latency_single"] = results["median_latency_single"];

    std::cout << config.access_range << " : " << results["latency_single"] << std::endl;

    memRes.deallocate(zero_buffer, sizeof(size_t), 1 << 22);
    return 1;
}

int main(int argc, char **argv)
{
    auto &benchmark_config = Prefetching::get().runtime_config;

    // clang-format off
    benchmark_config.add_options()
        ("memory_size", "Total memory allocated MiB", cxxopts::value<std::vector<int>>()->default_value("1024"))
        ("start_access_range", "start memory accesses range (in Bytes, max ~1GiB)", cxxopts::value<std::vector<int>>()->default_value("1024"))
        ("end_access_range", "end memory accesses range (in Bytes, max ~1GiB)", cxxopts::value<std::vector<int>>()->default_value("536870912"))
        ("growth_factor", "Factor with which the access range grows per iteration", cxxopts::value<std::vector<double>>()->default_value("1.1"))
        ("alloc_on_node", "Defines on which NUMA node the benchmark allocates memory", cxxopts::value<std::vector<NodeID>>()->default_value("0"))
        ("run_on_node", "Defines on which NUMA node the benchmark is run", cxxopts::value<std::vector<NodeID>>()->default_value("0"))
        ("repeats", "Number of memory accesses per configuration", cxxopts::value<std::vector<int>>()->default_value("10000000"))
        ("use_pointer_chase", "Use huge pages during allocation", cxxopts::value<std::vector<bool>>()->default_value("false"))
        ("use_explicit_huge_pages", "Use huge pages during allocation", cxxopts::value<std::vector<bool>>()->default_value("false"))
        ("madvise_huge_pages", "Madvise kernel to create huge pages on mem regions", cxxopts::value<std::vector<bool>>()->default_value("true"))
        ("generate_numa_matrix", "Automatically iterates over all possible alloc and run configurations", cxxopts::value<std::vector<bool>>()->default_value("false"))
        ("out", "Path on which results should be stored", cxxopts::value<std::vector<std::string>>()->default_value("latency_benchmark.json"));
    // clang-format on
    benchmark_config.parse(argc, argv);

    int benchmark_run = 0;

    std::vector<nlohmann::json> all_results;
    for (auto &runtime_config : benchmark_config.get_runtime_configs())
    {
        auto memory_size = convert<int>(runtime_config["memory_size"]);
        auto start_access_range = convert<int>(runtime_config["start_access_range"]);
        auto end_access_range = convert<int>(runtime_config["end_access_range"]);
        auto growth_factor = convert<double>(runtime_config["growth_factor"]);
        auto alloc_on_node = convert<NodeID>(runtime_config["alloc_on_node"]);
        auto run_on_node = convert<NodeID>(runtime_config["run_on_node"]);
        auto repeats = convert<int>(runtime_config["repeats"]);
        auto use_explicit_huge_pages = convert<bool>(runtime_config["use_explicit_huge_pages"]);
        auto madvise_huge_pages = convert<bool>(runtime_config["madvise_huge_pages"]);
        auto generate_numa_matrix = convert<bool>(runtime_config["generate_numa_matrix"]);
        auto out = convert<std::string>(runtime_config["out"]);
        auto use_pointer_chase = convert<bool>(runtime_config["use_pointer_chase"]);

        LBenchmarkConfig config = {
            memory_size,
            start_access_range,
            alloc_on_node,
            run_on_node,
            repeats,
            use_explicit_huge_pages,
            madvise_huge_pages,
        };

        std::vector<NodeID> alloc_on_nodes = {NodeID{alloc_on_node}};
        std::vector<NodeID> run_on_nodes = {NodeID{run_on_node}};

        if (generate_numa_matrix)
        {
            auto &numa_manager = Prefetching::get().numa_manager;
            alloc_on_nodes = numa_manager.active_nodes;
            run_on_nodes = numa_manager.active_nodes;
        }

        for (NodeID alloc_on : alloc_on_nodes)
        {
            config.alloc_on_node = alloc_on;
            void *buffer;
            auto memRes = StaticNumaMemoryResource(config.alloc_on_node, config.use_explicit_huge_pages, config.madvise_huge_pages);

            if (use_pointer_chase)
            {
                buffer = memRes.allocate(end_access_range, 1 << 22);
                memset(buffer, 1, end_access_range);
            }
            else
            {
                buffer = memRes.allocate(config.memory_size << 20, 1 << 22);
                memset(buffer, 0, config.memory_size << 20);
            }
            for (NodeID run_on : run_on_nodes)
            {
                config.run_on_node = run_on;

                std::cout << "alloc_on_node: " << config.alloc_on_node << " run_on_node: " << config.run_on_node << std::endl;

                for (int access_range = start_access_range; access_range <= end_access_range; access_range *= growth_factor)
                {
                    config.access_range = access_range;
                    nlohmann::json results;
                    results["config"]["memory_size"] = config.memory_size;
                    results["config"]["access_range"] = access_range;
                    results["config"]["alloc_on_node"] = alloc_on;
                    results["config"]["run_on_node"] = run_on;
                    results["config"]["use_explicit_huge_pages"] = config.use_explicit_huge_pages;
                    results["config"]["madvise_huge_pages"] = config.madvise_huge_pages;
                    results["config"]["use_pointer_chase"] = use_pointer_chase;
                    if (!use_pointer_chase)
                    {
                        latency_bench(config, results, reinterpret_cast<char *>(buffer));
                    }
                    else
                    {
                        pointer_chase(config, results, reinterpret_cast<size_t *>(buffer));
                    }
                    all_results.push_back(results);
                }
            }
            if (use_pointer_chase)
            {
                memRes.deallocate(buffer, end_access_range, 1 << 22);
            }
            else
            {
                memRes.deallocate(buffer, config.memory_size << 20, 1 << 22);
            }
        }

        auto results_file = std::ofstream{out};
        nlohmann::json intermediate_json;
        intermediate_json["results"] = all_results;
        results_file << intermediate_json.dump(-1) << std::flush;
    }

    return 0;
}
