include_directories(utils)

add_library(utils utils/profiler.cpp utils/numa_memory_resource.cpp)
target_link_libraries(utils nlohmann_json::nlohmann_json numa jemalloc)


add_library(hashmap hashmap.cpp)
target_link_libraries(hashmap PRIVATE utils)
add_library(random_access random_access.cpp)
target_link_libraries(random_access PRIVATE utils)
