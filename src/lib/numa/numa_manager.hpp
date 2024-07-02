#pragma once

#include <vector>
#include <optional>

#include "interleaving_numa_memory_resource.hpp"
#include "../types.hpp"

class NumaManager
{
public:
    NodeID number_cpus;
    NodeID number_nodes;
    std::vector<std::vector<NodeID>> node_to_cpus;
    std::vector<NodeID> cpu_to_node;
    std::optional<InterleavingNumaMemoryResource> interleaving_memory_resource;
    NumaManager();

private:
    void init_topology_info();
    void print_topology();
};
