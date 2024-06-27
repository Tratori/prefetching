#pragma once

#include <cstddef>
#include <memory_resource>
#include <iostream>

#include <jemalloc/jemalloc.h>

#include "static_numa_memory_resource.hpp"

StaticNumaMemoryResource::StaticNumaMemoryResource(NodeID target_numa_node) : target_numa_node_(target_numa_node){};

NodeID StaticNumaMemoryResource::node_id(void *p)
{
    return target_numa_node_;
}