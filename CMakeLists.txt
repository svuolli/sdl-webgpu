cmake_minimum_required(VERSION 3.24)
project(
    sdl-webgpu
    LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

find_package(SDL2 REQUIRED)

add_subdirectory(webgpu)
add_subdirectory(src)
add_subdirectory(demo)
