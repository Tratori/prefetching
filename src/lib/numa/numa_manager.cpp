#include <stdexcept>
#include "numa.h"
#include <iostream>

#include "numa_manager.hpp"

NumaManager::NumaManager()
{
    if (numa_available() < 0)
    {
        throw std::runtime_error("Numa library not available.");
    }

    if (numa_max_node() == 0)
    {
        std::cout << std::endl
                  << "\033[1;31m[WARNING]: only one NUMA node identified, NUMA bindings won't have any effect.\033[0m" << std::endl
                  << std::endl;
    }
    init_topology_info();
    print_topology();
    interleaving_memory_resource.emplace(number_nodes);
}

void NumaManager::init_topology_info()
{
    number_cpus = numa_num_task_cpus();
    number_nodes = numa_num_task_nodes();
    cpu_to_node.reserve(number_cpus);
    node_to_cpus.resize(number_nodes);
    for (NodeID numa_cpu = 0; numa_cpu < number_cpus; ++numa_cpu)
    {
        if (numa_bitmask_isbitset(numa_all_cpus_ptr, numa_cpu))
        {
            NodeID numa_node = numa_node_of_cpu(numa_cpu);
            cpu_to_node.push_back(numa_node);
            node_to_cpus[numa_node].push_back(numa_cpu);
        }
    }
}

void NumaManager::print_topology()
{
    auto allow_mem_nodes = numa_get_mems_allowed();
    std::cout << " --- NUMA Topology Information --- " << std::endl;
    std::cout << "number_cpus: " << number_cpus << "/" << numa_num_configured_cpus() << std::endl;
    std::cout << "number_nodes: " << number_nodes << "/" << numa_num_configured_nodes() << std::endl;
    for (NodeID node = 0; node < number_nodes; node++)
    {
        std::cout << "Node [" << node << "] (mem allowed=" << numa_bitmask_isbitset(allow_mem_nodes, node) << ") : ";
        for (auto cpu : node_to_cpus[node])
        {
            std::cout << cpu << " ";
        }
        std::cout << std::endl;
    }
}