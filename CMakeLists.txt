cmake_minimum_required(VERSION 3.10)

project(Prefetch)

set(CMAKE_CXX_STANDARD 20)


add_compile_options(-msse)


if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    add_custom_target(generate_asm
        COMMAND ${CMAKE_CXX_COMPILER} -S -fverbose-asm -o prefetch_latency.s ${CMAKE_SOURCE_DIR}/src/benchmark/prefetch_latency.cpp
        DEPENDS ${CMAKE_SOURCE_DIR}/src/benchmark/prefetch_latency.cpp
        COMMENT "Generating assembly code for prefetch_latency.cpp"
    )
endif()

add_subdirectory(third_party)
add_subdirectory(src)