#pragma once

#include <cstddef>
#include <memory_resource>

// #include <boost/container/pmr/memory_resource.hpp>
#include <jemalloc/jemalloc.h>

using NodeID = u_int16_t;

std::size_t get_page_size();
std::size_t calculate_allocated_pages(size_t size);

void *alloc(extent_hooks_t *extent_hooks, void *new_addr, size_t size, size_t alignment, bool *zero,
            bool *commit, unsigned arena_index);

/**
 * The base memory resource for NUMA memory allocation.
 *
 * We want a low overhead numa memory allocator, where we can tell on
 * which node data is stored by the pointer it self. Thus, initially a large
 * pool of memory is allocated aligned to the page size. Each page is then moved
 * to its specific node based on a simple (mathematic rule), e.g. (ptr / 4096) % num_nodes.
 */
class NumaMemoryResource : public std::pmr::memory_resource
{
public:
    // Constructor creating an arena for a specific node.
    explicit NumaMemoryResource();

    // Methods defined by memory_resource.
    void *do_allocate(std::size_t bytes, std::size_t alignment) override;

    /**
     * Entry point for deallocation behavior.
     */
    void do_deallocate(void *p, std::size_t bytes, std::size_t alignment) override;

    bool do_is_equal(const memory_resource &other) const noexcept override;

    static NodeID node_id(void *p);

protected:
    int32_t _allocation_flags{0};
};
