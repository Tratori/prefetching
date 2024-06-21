#pragma once

#include <boost/container/pmr/memory_resource.hpp>

#include "utils/singleton.hpp"
#include "utils/runtime_config.hpp"
#include "numa/numa_manager.hpp"

class Prefetching : public Singleton<Prefetching>
{
public:
    NumaManager numa_manager;
    RuntimeConfig runtime_config;

private:
    Prefetching();
    friend class Singleton;
};
