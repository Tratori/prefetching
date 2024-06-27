#pragma once

#include <cstddef>
#include <memory_resource>

// #include <boost/container/pmr/memory_resource.hpp>
#include <jemalloc/jemalloc.h>

#include "numa_memory_resource.hpp"

class StaticNumaMemoryResource : public NumaMemoryResource
{
public:
    // Constructor creating an arena for a specific node.
    explicit StaticNumaMemoryResource(NodeID target_numa_node);

    NodeID node_id(void *p);

protected:
    const NodeID target_numa_node_;
};
