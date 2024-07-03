#include <stdint.h>
#include <x86intrin.h>
#include <vector>
#include <random>
#include <iostream>
#include <thread>

#include "profiler.cpp"
#include "../types.hpp"

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
    else
    {
        __builtin_prefetch(ptr, 0, 3); // Prefetch to L1 cache
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

void pin_to_cpu(NodeID cpu)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    const auto return_code = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (return_code != 0)
    {
        throw std::runtime_error("pinning thread to cpu failed (return code: " + std::to_string(return_code) + ").");
    }
};

void initialize_pointer_chase(uint64_t *data, size_t size)
{
    std::vector<size_t> random_numbers(size);

    std::iota(random_numbers.begin(), random_numbers.end(), 0);
    auto rng = std::mt19937{42};
    std::shuffle(random_numbers.begin(), random_numbers.end(), rng);

    auto zero_it = std::find(random_numbers.begin(), random_numbers.end(), 0);
    unsigned curr = 0;
    for (auto behind_zero = zero_it + 1; behind_zero < random_numbers.end(); behind_zero++)
    {
        *(data + curr) = *behind_zero;
        curr = *behind_zero;
    }
    for (auto before_zero = random_numbers.begin(); before_zero <= zero_it; before_zero++)
    {
        *(data + curr) = *before_zero;
        curr = *before_zero;
    }
}