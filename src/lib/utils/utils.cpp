#include <stdint.h>
#if defined(X86_64)
#include <x86intrin.h>
#elif defined(AARCH64)
#include <atomic>
#endif
#include <vector>
#include <random>
#include <chrono>
#include <iostream>
#include <algorithm>
#include <thread>

#include "profiler.cpp"
#include "../types.hpp"

void wait_cycles(uint64_t x)
{
    for (int i = 0; i < x; ++i)
    {
#if defined(X86_64)
        _mm_pause();
#elif defined(AARCH64)
        __asm__ volatile("yield");
#endif
    };
}

inline unsigned long long read_cycles()
{
#if defined(X86_64)
    return __rdtsc();
#elif defined(AARCH64)
    uint64_t cycles;
    asm volatile("mrs %0, cntvct_el0" : "=r"(cycles));
    return cycles;
#endif
}

inline void lfence()
{
#if defined(X86_64)
    _mm_lfence();
#elif defined(AARCH64)
    std::atomic_thread_fence(std::memory_order::consume);
#endif
}

inline void sfence()
{
#if defined(X86_64)
    _mm_sfence();
#elif defined(AARCH64)
    std::atomic_thread_fence(std::memory_order::release);
#endif
}

const uint64_t l1_prefetch_latency = 44;

static uint64_t sampling_counter = 0;

inline bool is_in_tlb_and_prefetch(const void *ptr)
{
    uint64_t start, end;

    start = read_cycles();
    lfence();
    asm volatile("" ::: "memory");

    __builtin_prefetch(ptr, 0, 3); // Prefetch to L1 cache

    asm volatile("" ::: "memory");
    lfence();
    end = read_cycles();

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
        start = read_cycles();
        lfence();
        asm volatile("" ::: "memory");

        __builtin_prefetch(ptr, 0, 3); // Prefetch to L1 cache

        asm volatile("" ::: "memory");
        lfence();
        end = read_cycles();

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
    std::vector<uint64_t> random_numbers(size);

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

    // verify pointer chase:
    // auto jumper = data;
    // auto data_first = reinterpret_cast<uint64_t>(data);
    // uint64_t counter = 1;
    // while (*jumper != 0)
    //{
    //    jumper = reinterpret_cast<uint64_t *>(data + *jumper);
    //    counter++;
    //}
    // if (counter != size)
    //{
    //    throw std::runtime_error("pointer chase init failed. Expected jumps : " + std::to_string(size) + " actual jumps: " + std::to_string(counter));
    //}
}

// --- "Work" taken from https://dl.acm.org/doi/10.1145/3662010.3663451 ---

inline std::uint32_t murmur_32_scramble(std::uint32_t k)
{
    /// Murmur3 32bit https://en.wikipedia.org/wiki/MurmurHash
    k *= 0xcc9e2d51;
    k = (k << 15) | (k >> 17);
    k *= 0x1b873593;
    return k;
}

inline std::uint32_t murmur_32(std::uint32_t val)
{
    /// Murmur3 32bit https://en.wikipedia.org/wiki/MurmurHash
    auto h = 19553029U;

    /// Scramble
    h ^= murmur_32_scramble(val);
    h = (h << 13U) | (h >> 19U);
    h = h * 5U + 0xe6546b64;

    /// Finalize
    h ^= sizeof(std::uint32_t);
    h ^= h >> 16U;
    h *= 0x85ebca6b;
    h ^= h >> 13U;
    h *= 0xc2b2ae35;
    h ^= h >> 16U;

    return h;
}

// --- End "Work" ---

size_t align_to_power_of_floor(size_t p, size_t align)
{
    return p & ~(align - 1);
}

template <typename T>
size_t get_data_size_in_bytes(std::pmr::vector<T> &vec)
{
    return vec.size() * sizeof(T);
}

auto findMedian(auto &container,
                int n)
{

    if (n % 2 == 0)
    {

        nth_element(container.begin(),
                    container.begin() + n / 2,
                    container.end());

        nth_element(container.begin(),
                    container.begin() + (n - 1) / 2,
                    container.end());

        return (container[(n - 1) / 2] + container[n / 2]) / 2.0;
    }

    else
    {

        nth_element(container.begin(),
                    container.begin() + n / 2,
                    container.end());

        return container[n / 2];
    }
}

auto get_steady_clock_min_duration(size_t repetitions)
{
    // warm up
    for (size_t i = 0; i < 50'000'000; ++i)
    {
        asm volatile("" ::: "memory");
        auto start = std::chrono::steady_clock::now();
        asm volatile("" ::: "memory");
        auto end = std::chrono::steady_clock::now();
        asm volatile("" ::: "memory");
    }

    std::vector<std::chrono::duration<double>> durations(repetitions);
    for (size_t i = 0; i < repetitions; ++i)
    {
        asm volatile("" ::: "memory");
        auto start = std::chrono::steady_clock::now();
        asm volatile("" ::: "memory");
        auto end = std::chrono::steady_clock::now();
        asm volatile("" ::: "memory");
        durations[i] = end - start;
    }
    return findMedian(durations, durations.size());
}

void ensure(auto exp, auto message)
{
    if (!exp)
    {
        throw std::runtime_error(message);
    }
}
/*
    Even when using huge pages, l2 tlb misses are still a thing somehow.
    To avoid having to pay these tlb misses during prefetches, we ignore the first x bytes of each page and fill them with 0.
    Later, when want to prefetch the random pointers, we touch these first bytes of all to be resolved pointers first.
    We write one pointer per cacheline and one pointer always points to the start of a cacheline.
    layout of a single page:

        | pad_bytes | cacheline 1 | ... | cacheline n | remainder |

*/
void initialize_padded_pointer_chase(auto &data_vector, size_t total_byte_size, size_t pad_bytes, size_t page_size, size_t cache_line_size)
{
    ensure(pad_bytes % cache_line_size == 0, "Padding must be multiple of cache line size");

    size_t pointers_per_page = (page_size - pad_bytes) / cache_line_size;
    size_t pages = total_byte_size / page_size;

    ensure(data_vector.size() == total_byte_size, "Data vector is of wrong size");

    memset(data_vector.data(), 0, total_byte_size);
    std::vector<char *> random_pointers;
    random_pointers.reserve(pointers_per_page * pages);

    for (size_t page = 0; page < pages; ++page)
    {
        for (size_t pointer = 0; pointer < pointers_per_page; ++pointer)
        {
            random_pointers.push_back(data_vector.data() + page * page_size + pad_bytes + pointer * cache_line_size);
        }
    }

    auto rng = std::mt19937{std::random_device{}()};
    std::shuffle(random_pointers.begin(), random_pointers.end(), rng);

    for (size_t i = 0; i < random_pointers.size(); ++i)
    {
        char **current_pointer = reinterpret_cast<char **>(random_pointers[i]);
        char *next_pointer = random_pointers[(i + 1) % random_pointers.size()];
        *current_pointer = next_pointer;
    }

    // Verify pointer chase:
    char *start = data_vector.data() + pad_bytes;
    char *curr = start;
    uint64_t counter = 0;
    do
    {
        curr = *reinterpret_cast<char **>(curr);
        counter++;
    } while (curr != start);

    if (counter != pointers_per_page * pages)
    {
        throw std::runtime_error("Pointer chase init failed. Expected jumps: " + std::to_string(pointers_per_page * pages) + " actual jumps: " + std::to_string(counter));
    }
}