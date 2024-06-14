#include <stdint.h>
#include <x86intrin.h>
#include <vector>
#include <iostream>

#include "profiler.cpp"

const uint64_t l1_prefetch_latency = 44;

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

inline bool is_in_tlb_prefetch_profile(const void *ptr, size_t &step, PrefetchProfiler &profiler, bool &assume_cached)
{
    uint64_t start, end;
    if (step == 1)
    {
        start = __rdtsc();
        _mm_lfence();
        asm volatile("" ::: "memory");

        __builtin_prefetch(ptr, 0, 3); // Prefetch to L1 cache

        asm volatile("" ::: "memory");
        _mm_lfence();
        end = __rdtsc();

        bool is_hit = (end - start) <= l1_prefetch_latency;
        profiler.note_cache_hit_or_miss(is_hit, step);
        profiler.sampled_latency_store(end - start);

        assume_cached = is_hit;
    }

    step++;
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