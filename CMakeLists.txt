cmake_minimum_required(VERSION 3.10)

project(Prefetch)

set(CMAKE_CXX_STANDARD 20)


add_compile_options(-msse)

add_subdirectory(third_party)
add_subdirectory(src)