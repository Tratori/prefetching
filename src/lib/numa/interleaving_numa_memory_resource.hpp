#pragma once

#include <cstddef>
#include <memory_resource>

// #include <boost/container/pmr/memory_resource.hpp>
#include <jemalloc/jemalloc.h>

#include "numa_memory_resource.hpp"

class InterleavingNumaMemoryResource : public NumaMemoryResource
{
public:
    explicit InterleavingNumaMemoryResource(NodeID num_numa_nodes);

    NodeID node_id(void *p) override;
    void move_pages_policed(void *p, size_t size) override;

protected:
    const NodeID num_numa_nodes_;
};
