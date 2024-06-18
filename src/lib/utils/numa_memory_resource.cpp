#include <cstddef>
#include <stdexcept>
#include <sys/mman.h>
#include <numaif.h>
#include <vector>

#include <sstream>

#include <boost/container/pmr/memory_resource.hpp>
#include <jemalloc/jemalloc.h>

#include "numa_memory_resource.hpp"

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

void *NumaMemoryResource::alloc(extent_hooks_t *extent_hooks, void *new_addr, size_t size, size_t alignment, bool *zero,
                                bool *commit, unsigned arena_index)
{
    // map return addresses aligned to page size
    void *addr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    if (addr == nullptr)
    {
        throw std::runtime_error("Failed to mmap pages.");
    }

    unsigned long num_pages = calculate_allocated_pages(size);
    std::vector<void *> page_pointers(num_pages);
    std::vector<int> nodes(num_pages);
    std::vector<int> status(num_pages);
    for (int i = 0; i < num_pages; ++i)
    {
        page_pointers[i] = reinterpret_cast<char *>(addr) + (i * PAGE_SIZE);
        nodes[i] = NumaMemoryResource::node_id(reinterpret_cast<char *>(addr) + (i * PAGE_SIZE));
    }
    if (move_pages(0, num_pages, page_pointers.data(), nodes.data(), status.data(), 0) != 0)
    {
        throw std::runtime_error("move_pages failed");
    }
    for (int i : status)
    {
        if (i != 0)
        {
            throw std::runtime_error("Page could not be moved. err: " + std::to_string(i));
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