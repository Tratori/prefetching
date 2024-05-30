include_directories(
    ${PROJECT_SOURCE_DIR}/src/lib
)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/benchmark)

add_executable(hashmap_benchmark hashmap_benchmark.cpp)

target_link_libraries(hashmap_benchmark hashmap benchmark::benchmark)