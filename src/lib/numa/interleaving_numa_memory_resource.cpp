#pragma once

#include <cstddef>
#include <memory_resource>
#include <iostream>

#include <jemalloc/jemalloc.h>

#include "interleaving_numa_memory_resource.hpp"

InterleavingNumaMemoryResource::InterleavingNumaMemoryResource(NodeID num_numa_nodes) : num_numa_nodes_(num_numa_nodes){};

NodeID InterleavingNumaMemoryResource::node_id(void *p)
{
    // 2/4 (not sure rn, doesnt really matter) adjacent pages will be on the same node (i hope lol)
    return (reinterpret_cast<uint64_t>(p) >> 14) % num_numa_nodes_;
}