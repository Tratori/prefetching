#pragma once

#include <cstddef>
#include <memory_resource>
#include <unistd.h>

// #include <boost/container/pmr/memory_resource.hpp>
#include <jemalloc/jemalloc.h>

using NodeID = u_int16_t;

inline std::size_t get_page_size()
{
    return static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
}
std::size_t calculate_allocated_pages(size_t size);

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
    explicit NumaMemoryResource(bool use_huge_pages = false);

    // Methods defined by memory_resource.
    void *do_allocate(std::size_t bytes, std::size_t alignment) override;

    /**
     * Entry point for deallocation behavior.
     */
    void do_deallocate(void *p, std::size_t bytes, std::size_t alignment) override;

    bool do_is_equal(const memory_resource &other) const noexcept override;

    virtual NodeID node_id(void *p) = 0;
    virtual void move_pages_policed(void *p, size_t size) = 0;

    static void *alloc(extent_hooks_t *extent_hooks, void *new_addr, size_t size, size_t alignment, bool *zero,
                       bool *commit, unsigned arena_index);

protected:
    extent_hooks_t extentHooks_;
    bool _use_huge_pages;
    int32_t _allocation_flags{0};
};
