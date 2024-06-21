add_library(utils profiler.cpp runtime_config.cpp)
target_link_libraries(utils nlohmann_json::nlohmann_json cxxopts)
