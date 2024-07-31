cmake_minimum_required(VERSION 3.10)

project(Prefetch)

set(CMAKE_CXX_STANDARD 20)


message(STATUS "ARCH: ${CMAKE_SYSTEM_PROCESSOR}")
set(ARCH CMAKE_SYSTEM_PROCESSOR)
if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    add_custom_target(generate_asm
        COMMAND ${CMAKE_CXX_COMPILER} -S -fverbose-asm -o prefetch_latency.s ${CMAKE_SOURCE_DIR}/src/benchmark/prefetch_latency.cpp
        DEPENDS ${CMAKE_SOURCE_DIR}/src/benchmark/prefetch_latency.cpp
        COMMENT "Generating assembly code for prefetch_latency.cpp"
    )
endif()

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH}) # To allow CMake to locate our Find*.cmake files


set(DEFAULT_LIB_DIRS $ENV{HOME}/local /opt/local /usr/local /usr)
find_package(Boost REQUIRED)
find_package(Numa REQUIRED)


add_subdirectory(third_party)
add_subdirectory(src)