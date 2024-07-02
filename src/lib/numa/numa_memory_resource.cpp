#include <cstddef>
#include <stdexcept>
#include <sys/mman.h>
#include <numaif.h>
#include <numa.h>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <errno.h>

#include <boost/container/pmr/memory_resource.hpp>
#include <jemalloc/jemalloc.h>

#include "numa_memory_resource.hpp"

#define USE_MBIND // vs. numa_move_pages

std::unordered_map<unsigned, NumaMemoryResource *> arena_to_resource_map;

static const auto PAGE_SIZE = get_page_size();

std::size_t calculate_allocated_pages(size_t size)
{
    return (size + PAGE_SIZE - 1) / PAGE_SIZE;
}

NumaMemoryResource::NumaMemoryResource(bool use_huge_pages) : _use_huge_pages(use_huge_pages)
{
    auto arena_id = uint32_t{0};
    auto size = sizeof(arena_id);
    if (mallctl("arenas.create", static_cast<void *>(&arena_id), &size, nullptr, 0) != 0)
    {
        throw std::runtime_error("Could not create arena");
    }

    std::ostringstream hooks_key;
    hooks_key << "arena." << arena_id << ".extent_hooks";
    extent_hooks_t *hooks;
    size = sizeof(hooks);
    // Read the existing hooks
    if (auto ret = mallctl(hooks_key.str().c_str(), &hooks, &size, nullptr, 0))
    {
        throw std::runtime_error("Unable to get the hooks");
    }

    // Set the custom hook
    extentHooks_ = *hooks;
    extentHooks_.alloc = &alloc;
    extent_hooks_t *new_hooks = &extentHooks_;
    if (auto ret = mallctl(
            hooks_key.str().c_str(),
            nullptr,
            nullptr,
            &new_hooks,
            sizeof(new_hooks)))
    {
        throw std::runtime_error("Unable to set the hooks");
    }

    _allocation_flags = MALLOCX_ARENA(arena_id) | MALLOCX_TCACHE_NONE;
    arena_to_resource_map[arena_id] = this;
};

void *NumaMemoryResource::do_allocate(std::size_t bytes, std::size_t alignment)
{
    const auto addr = mallocx(bytes, _allocation_flags | MALLOCX_ALIGN(alignment));
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

void *NumaMemoryResource::alloc(extent_hooks_t *extent_hooks, void *new_addr, size_t size, size_t alignment, bool *zero,
                                bool *commit, unsigned arena_index)
{
    // map return addresses aligned to page size
#ifdef USE_MBIND
    auto mmap_flags = MAP_PRIVATE | MAP_ANONYMOUS;
#else
    auto mmap_flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE;
#endif
    auto memory_resource = arena_to_resource_map[arena_index];
    if (memory_resource->_use_huge_pages)
    {
        mmap_flags |= MAP_HUGETLB;
    }

    void *addr = mmap(nullptr, size, PROT_READ | PROT_WRITE, mmap_flags, -1, 0);
    if (addr == nullptr)
    {
        throw std::runtime_error("Failed to mmap pages.");
    }
    unsigned long num_pages = calculate_allocated_pages(size);

    memory_resource->move_pages_policed(addr, size);

    return addr;
}