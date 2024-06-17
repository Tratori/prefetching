#include <stdint.h>
#include <x86intrin.h>
#include <vector>
#include <iostream>
#include <coroutine>

#include "profiler.cpp"
#include "coroutine.hpp"

const uint64_t l1_prefetch_latency = 44;
const uint64_t sampling_freq = 1'000;
static uint64_t sampling_counter = 0;

inline bool is_in_tlb_and_prefetch(const void *ptr)
{
    uint64_t start, end;

    start = __rdtsc();
    _mm_lfence();
    asm volatile("" ::: "memory");

    __builtin_prefetch(ptr, 0, 3); // Prefetch to L1 cache

    asm volatile("" ::: "memory");
    _mm_lfence();
    end = __rdtsc();

    // sampling_counter++;
    // if ((sampling_counter & 0xFFFF) == 0xFFFF)
    //{
    //     std::cout << end - start << "," << std::endl;
    // }

    return (end - start) <= l1_prefetch_latency;
}

inline int init_profiler(PrefetchProfiler &profiler, uint64_t &last_tsc)
{
    profiler.sampling_counter++;
    if (profiler.sampling_counter & profiler.sampling_mask == profiler.sampling_mask)
    {
        last_tsc = __rdtsc();
        return profiler.sample_id++ % profiler.latencies.size();
    }
    return -1;
}

inline bool is_in_tlb_prefetch_profile(const void *ptr, size_t &step, int sample_id, uint64_t &last_tsc, PrefetchProfiler &profiler, bool &assume_cached)
{
    if (sample_id != -1)
    {
        uint64_t now = __rdtsc();
        profiler.latencies.at(sample_id).at(step) = now - last_tsc;
        last_tsc = now;
        step++;
        return true;
    }
    else
    {
        step++;

        __builtin_prefetch(ptr, 0, 3); // Prefetch to L1 cache
    }

    return assume_cached;
}

template <typename T>
class CircularBuffer
{
private:
    std::vector<T> buffer;
    size_t capacity;
    size_t next_index;

public:
    CircularBuffer(size_t capacity) : capacity(capacity), next_index(0)
    {
        buffer.resize(capacity);
    }

    T &next_state()
    {
        T &state = buffer[next_index];
        next_index = (next_index + 1) % capacity;
        return state;
    }
};