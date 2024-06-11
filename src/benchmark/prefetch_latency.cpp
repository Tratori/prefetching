#include <stdint.h>
#include <x86intrin.h>
#include <random>
#include <iostream>

const uint64_t GIBIBYTE = 1024 * 1024 * 1024;
const uint64_t MIBIBYTE = 1024 * 1024;
const uint64_t REPETITIONS = 100;
const uint64_t LINEAR_AHEAD_LOADS = 64;
const uint64_t PADDING_END = 3;
const uint64_t DATA_SIZE = GIBIBYTE;

uint8_t data[DATA_SIZE];

void wait_cycles(uint64_t x)
{
    for (int i = 0; i < x; ++i)
    {
        _mm_pause();
    };
}

uint64_t measure_prefetch_latency(const void *ptr)
{
    uint64_t start, end;

    start = __rdtsc();
    _mm_lfence();
    asm volatile("" ::: "memory");

    __builtin_prefetch(ptr, 0, 3); // Prefetch to L1 cache

    asm volatile("" ::: "memory");
    _mm_lfence();
    end = __rdtsc();

    return end - start;
}

uint64_t measure_load_latency(void *ptr, volatile uint64_t &dummy_sum)
{
    uint64_t start, end;

    start = __rdtsc();
    _mm_lfence();
    asm volatile("" ::: "memory");

    dummy_sum = dummy_sum + *reinterpret_cast<uint64_t *>(ptr);

    asm volatile("" ::: "memory");
    _mm_lfence();
    end = __rdtsc();

    return end - start;
}

uint64_t measure_prefetch_latency_verbose(const void *ptr)
{
    uint64_t latency = measure_prefetch_latency(ptr);
    const uint64_t cache_hit_threshold = 37;
    if (latency < cache_hit_threshold)
    {
        std::cout << "Data was in cache (cache hit)" << std::endl;
    }
    else
    {
        std::cout << "Data was not in cache (cache miss)" << std::endl;
    }
    printf("Prefetch latency: %lu cycles\n", latency);
    return latency;
}

uint64_t measure_load_latency_verbose(void *ptr, volatile uint64_t &dummy_sum)
{
    auto lat = measure_load_latency(ptr, dummy_sum);
    std::cout << "Load Latency was: " << lat << std::endl;
    return lat;
}

int main()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(LINEAR_AHEAD_LOADS, DATA_SIZE - 1 - PADDING_END);

    for (int i = 0; i < REPETITIONS; i++)
    {
        int random_number = dis(gen);
        uint64_t latency = measure_prefetch_latency_verbose(&data[random_number]); // most likely uncached data
    }

    std::cout << "-------    Expecting Cache Hits now -------" << std::endl;
    volatile uint64_t dummy_sum = 0;
    for (int i = 0; i < REPETITIONS; i++)
    {
        int random_number = dis(gen);
        auto ptr = &data[random_number];
        __builtin_prefetch(ptr, 0, 3); // Prefetch to L1 cache

        for (int i = 0; i < LINEAR_AHEAD_LOADS; ++i)
        {
            dummy_sum = dummy_sum + data[random_number - LINEAR_AHEAD_LOADS + i]; // attempt to also trigger hardware prefetcher
        }
        wait_cycles(200);
        uint64_t latency = measure_prefetch_latency_verbose(ptr); // most likely cached data
    }

    std::cout << "------- Measuring Load Latencies now -------" << std::endl;

    for (int i = 0; i < REPETITIONS; i++)
    {
        int random_number = dis(gen);
        uint64_t latency = measure_load_latency_verbose(&data[random_number], dummy_sum); // most likely uncached data
    }

    std::cout << "-------    Expecting Cache Hits now -------" << std::endl;
    for (int i = 0; i < REPETITIONS; i++)
    {
        int random_number = dis(gen);
        auto ptr = &data[random_number];
        __builtin_prefetch(ptr, 0, 3); // Prefetch to L1 cache

        for (int i = 0; i < LINEAR_AHEAD_LOADS; ++i)
        {
            dummy_sum = dummy_sum + data[random_number - LINEAR_AHEAD_LOADS + i]; // attempt to also trigger hardware prefetcher
        }
        wait_cycles(200);
        uint64_t latency = measure_load_latency_verbose(ptr, dummy_sum); // most likely cached data
    }

    return 0;
}