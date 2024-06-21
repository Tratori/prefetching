#pragma once

#include <vector>

using NumaNode = int;
using NumaCPU = int;

class NumaManager
{
public:
    NumaCPU number_cpus;
    NumaNode number_nodes;
    std::vector<std::vector<NumaCPU>> node_to_cpus;
    std::vector<NumaNode> cpu_to_node;

    NumaManager();

private:
    void init_topology_info();
    void print_topology();
};
