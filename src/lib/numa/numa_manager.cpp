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
}

void NumaManager::init_topology_info()
{
    number_cpus = numa_num_configured_cpus();
    number_nodes = numa_num_configured_nodes();
    cpu_to_node.reserve(number_cpus);
    node_to_cpus.resize(number_nodes);
    for (NumaCPU numa_cpu = 0; numa_cpu < number_cpus; ++numa_cpu)
    {
        NumaNode numa_node = numa_node_of_cpu(numa_cpu);
        cpu_to_node.push_back(numa_node);
        node_to_cpus[numa_node].push_back(numa_cpu);
    }
}

void NumaManager::print_topology()
{
    std::cout << " --- NUMA Topology Information --- " << std::endl;
    std::cout << "number_cpus: " << number_cpus << std::endl;
    std::cout << "number_nodes: " << number_nodes << std::endl;
    for (NumaNode node = 0; node < number_nodes; node++)
    {
        std::cout << "Node " << node << " ";
        for (auto cpu : node_to_cpus[node])
        {
            std::cout << cpu << " ";
        }
        std::cout << std::endl;
    }
}