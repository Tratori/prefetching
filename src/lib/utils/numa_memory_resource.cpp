#include <cstddef>
#include <stdexcept>
#include <sys/mman.h>
#include <numaif.h>
#include <vector>

#include <boost/container/pmr/memory_resource.hpp>
#include <jemalloc/jemalloc.h>

#include "numa_memory_resource.hpp"

static extent_hooks_t _hooks{
    .alloc = alloc};

inline std::size_t get_page_size()
{
    return static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
}

static const auto PAGE_SIZE = get_page_size();

std::size_t calculate_allocated_pages(size_t size)
{
    return (size + PAGE_SIZE - 1) / PAGE_SIZE;
}

NumaMemoryResource::NumaMemoryResource()
{
    auto arena_id = uint32_t{0};
    auto size = sizeof(arena_id);
    if (mallctl("arenas.create", static_cast<void *>(&arena_id), &size, nullptr, 0) != 0)
    {
        perror("Could not create arena");
    }

    char command[64];
    snprintf(command, sizeof(command), "arena.%u.extent_hooks", arena_id);
    if (mallctl(command, nullptr, nullptr, static_cast<void *>(&_hooks), sizeof(extent_hooks_t *)) != 0)
    {
        perror("mallctl failed");
    }

    _allocation_flags = MALLOCX_ARENA(arena_id) | MALLOCX_TCACHE_NONE;
};

void *NumaMemoryResource::do_allocate(std::size_t bytes, std::size_t alignment)
{
    const auto addr = mallocx(bytes, _allocation_flags);
    return addr;
}

void NumaMemoryResource::do_deallocate(void *p, std::size_t bytes, std::size_t alignment)
{
    dallocx(p, _allocation_flags);
}

bool NumaMemoryResource::do_is_equal(const memory_resource &other) const noexcept
{
    return &other == this;
}

NodeID NumaMemoryResource::node_id(void *p)
{
    return 0;
}

void *alloc(extent_hooks_t *extent_hooks, void *new_addr, size_t size, size_t alignment, bool *zero,
            bool *commit, unsigned arena_index)
{
    // map return addresses aligned to page size
    void *addr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (addr == nullptr)
    {
        perror("Failed to mmap pages.");
    }

    auto num_pages = calculate_allocated_pages(size);
    std::vector<void *> page_pointers;
    std::vector<int> nodes;
    std::vector<int> status;
    page_pointers.reserve(num_pages);
    nodes.reserve(num_pages);
    status.resize(num_pages);
    for (int i = 0; i < num_pages; ++i)
    {
        page_pointers.emplace_back(reinterpret_cast<char *>(addr) + (i * PAGE_SIZE));
        nodes.emplace_back(NumaMemoryResource::node_id(reinterpret_cast<char *>(addr) + (i * PAGE_SIZE)));
    }
    if (move_pages(0, num_pages, page_pointers.data(), nodes.data(), status.data(), MPOL_MF_MOVE_ALL) != 0)
    {
        perror("move pages failed");
    }
    for (int i : status)
    {
        if (i != 0)
        {
            perror("page could not be moved");
        }
    }
    // do
    //{
    //     mbind(reinterpret_cast<char *>(addr) + (i * PAGE_SIZE), PAGE_SIZE, MPOL_BIND, /* nodemask */, /* num_numa_nodes */, 0);
    //     ++i;
    // }
    // while (i * PAGE_SIZE < size);
    return addr;
}