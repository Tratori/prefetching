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
    std::vector<NodeID> active_nodes; // As in: Cpus on that node are available.
    std::vector<std::vector<std::vector<NodeID>>> socket_and_core_id_to_cpu;
    std::vector<NodeID> cpu_to_core_id;
    std::optional<InterleavingNumaMemoryResource> interleaving_memory_resource;
    NumaManager();

private:
    void init_topology_info();
    void print_topology();
};
