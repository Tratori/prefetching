include_directories(utils)

add_library(utils utils/profiler.cpp)
target_link_libraries(utils nlohmann_json::nlohmann_json)


add_library(hashmap hashmap.cpp)
target_link_libraries(hashmap utils)
add_library(random_access random_access.cpp)
target_link_libraries(random_access utils)
