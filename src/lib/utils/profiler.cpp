#pragma once

#include <vector>
#include <nlohmann/json.hpp>

class StepSpecifier
{
public:
    u_int64_t misses = 0;
    u_int64_t hits = 0;

    StepSpecifier(){};
    void reset()
    {
        misses = 0;
        hits = 0;
    }
};

class PrefetchProfiler
{
public:
    std::vector<StepSpecifier> classifications;
    std::vector<std::vector<uint16_t>> latencies;
    uint64_t sampling_mask = 1023; // store every sampling_mask-th latency

    uint64_t sampling_counter; // either manually handle, or use sampled_<x> functions
    size_t sample_id;
    size_t step_id;

    PrefetchProfiler(int maxPrefetches = 30, int samples = 100)
    {
        classifications.resize(maxPrefetches, {});
        latencies.resize(samples, std::vector<uint16_t>(maxPrefetches, 0));
        sample_id = 0;
        step_id = 0;
    }

    void sampled_latency_store(uint16_t latency)
    {
        if (sampling_counter & sampling_mask)
        {
            latencies.at(sample_id).at(step_id) = latency;
            sample_id = (sample_id + 1) % uint64_t{latencies.size()};
        }
        sampling_counter++;
    }

    void miss(size_t step)
    {
        classifications.at(step).misses++;
    }
    void hit(size_t step)
    {
        classifications.at(step).hits++;
    }
    void note_cache_hit_or_miss(bool is_hit, size_t step)
    {
        if (is_hit)
        {
            hit(step);
        }
        else
        {
            miss(step);
        }
    }

    std::vector<uint64_t> get_hits()
    {
        std::vector<uint64_t> hits(classifications.size());
        std::transform(classifications.begin(), classifications.end(), hits.begin(), [](const auto &prefetchCount)
                       { return prefetchCount.hits; });
        return hits;
    }

    std::vector<uint64_t> get_misses()
    {
        std::vector<uint64_t> misses(classifications.size());
        std::transform(classifications.begin(), classifications.end(), misses.begin(), [](const auto &prefetchCount)
                       { return prefetchCount.misses; });
        return misses;
    }

    nlohmann::json return_metrics()
    {
        nlohmann::json json{
            {"hits", get_hits()},
            {"misses", get_misses()},
            {"depth", classifications.size()},
            {"latencies", latencies}};
        return json;
    }

    void reset()
    {
        for (auto &spec : classifications)
        {
            spec.reset();
        }
        latencies.resize(latencies.size(), std::vector<uint16_t>(classifications.size(), 0));
        sample_id = 0;
        step_id = 0;
    }
};