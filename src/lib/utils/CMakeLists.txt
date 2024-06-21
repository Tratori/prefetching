add_library(utils profiler.cpp)
target_link_libraries(utils nlohmann_json::nlohmann_json)
