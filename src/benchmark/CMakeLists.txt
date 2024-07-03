include_directories(
    ${PROJECT_SOURCE_DIR}/src/lib
    utils
)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/benchmark)

add_executable(hashmap_benchmark hashmap_benchmark.cpp)

target_link_libraries(hashmap_benchmark hashmap prefetching)

add_executable(prefetch_latency prefetch_latency.cpp)

target_link_libraries(prefetch_latency nlohmann_json::nlohmann_json)

add_executable(random_read_benchmark random_read_benchmark.cpp)

target_link_libraries(random_read_benchmark random_access)

add_executable(tree_simulation tree_simulation_benchmark.cpp)

target_link_libraries(tree_simulation prefetching)

add_executable(lfb_size lfb_size_benchmark.cpp)

target_link_libraries(lfb_size prefetching)

add_executable(pc pointer_chasing.cpp)

target_link_libraries(pc prefetching)
