add_library(prefetching_numa numa_manager.cpp numa_memory_resource.cpp)

target_include_directories(prefetching_numa SYSTEM PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/../../../third_party/jemalloc/include)
target_link_libraries(prefetching_numa numa custom_jemalloc)