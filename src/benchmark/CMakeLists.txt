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

