add_subdirectory(utils)
add_subdirectory(numa)

include_directories(utils)

add_library(prefetching prefetching.cpp)
target_link_libraries(prefetching PUBLIC utils prefetching_numa)

add_library(hashmap hashmap.cpp)
target_include_directories(hashmap SYSTEM PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/../../third_party/jemalloc/include)
target_link_libraries(hashmap PRIVATE prefetching)
add_library(random_access random_access.cpp)
target_link_libraries(random_access PRIVATE prefetching)
