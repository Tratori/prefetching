#pragma once

#include <cstddef>
#include <memory_resource>
#include <numaif.h>
#include <numa.h>
#include <iostream>

#include <jemalloc/jemalloc.h>

#include "static_numa_memory_resource.hpp"

StaticNumaMemoryResource::StaticNumaMemoryResource(NodeID target_numa_node) : target_numa_node_(target_numa_node){};

NodeID StaticNumaMemoryResource::node_id(void *p)
{
    return target_numa_node_;
}

void StaticNumaMemoryResource::move_pages_policed(void *p, size_t size)
{
    const auto max_node = numa_max_node();
    if (max_node == 0)
    {
        return;
    }
    auto bitmask = numa_bitmask_alloc(numa_num_configured_cpus());
    const auto target_node_id = node_id(p);
    numa_bitmask_setbit(bitmask, target_node_id);
    auto ret = mbind(reinterpret_cast<char *>(p), size, MPOL_BIND, bitmask->maskp, max_node, 0);
    if (ret != 0)
    {
        throw std::runtime_error("mbind failed with " + std::to_string(ret) + " errno: " + strerror(errno));
    }
    numa_bitmask_clearbit(bitmask, target_node_id);
    numa_bitmask_free(bitmask);
}