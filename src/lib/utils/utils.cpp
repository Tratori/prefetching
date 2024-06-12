#include <stdint.h>
#include <x86intrin.h>
#include <vector>

const uint64_t l1_prefetch_latency = 44;

bool is_cached_l1_prefetch(const void *ptr)
{
    uint64_t start, end;

    start = __rdtsc();
    _mm_lfence();
    asm volatile("" ::: "memory");

    __builtin_prefetch(ptr, 0, 3); // Prefetch to L1 cache

    asm volatile("" ::: "memory");
    _mm_lfence();
    end = __rdtsc();

    return (end - start) <= l1_prefetch_latency;
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