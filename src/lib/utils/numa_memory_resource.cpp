#include <boost/container/pmr/memory_resource.hpp>
#include <cstddef>
#include <stdexcept>
#include <sys/mman.h>

using NodeID = u_int16_t;

inline std::size_t get_page_size()
{
    return static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
}

/**
 * The base memory resource for NUMA memory allocation.
 *
 * We want a low overhead numa memory allocator, where we can tell on
 * which node data is stored by the pointer it self. Thus, initially a large
 * pool of memory is allocated aligned to the page size. Each page is then moved
 * to its specific node based on a simple (mathematic rule), e.g. (ptr / 4096) % num_nodes.
 */
class NumaMemoryResource : public boost::container::pmr::memory_resource
{
public:
    // Constructor creating an arena for a specific node.
    explicit NumaMemoryResource(){};

    // Methods defined by memory_resource.
    void *do_allocate(std::size_t bytes, std::size_t alignment) override;

    /**
     * Entry point for deallocation behavior.
     */
    void do_deallocate(void *p, std::size_t bytes, std::size_t alignment) override;
    bool do_is_equal(const memory_resource &other) const noexcept override;
    NodeID node_id(void *p);

protected:
    int32_t _allocation_flags{0};
};
