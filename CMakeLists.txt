cmake_minimum_required(VERSION 3.24)
project(
    sdl-webgpu
    LANGUAGES C)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

option(SDL_WEBGPU_USE_CPM "Use CPM for package management" OFF)
option(SDL_WEBGPU_BUILD_DEMO "Build demo app" ON)

if(SDL_WEBGPU_USE_CPM)
    include(cmake/CPM.cmake)
    CPMAddPackage(
        NAME SDL2
        GIT_TAG release-2.28.1
        GITHUB_REPOSITORY libsdl-org/sdl
        OPTIONS "SDL_SHARED OFF" "SDL_STATIC ON")
    CPMAddPackage(
        NAME glm
        GIT_TAG 0.9.9.8
        GITHUB_REPOSITORY g-truc/glm)
else()
    find_package(SDL2 REQUIRED)
    find_package(glm REQUIRED)
endif()

add_subdirectory(webgpu)
add_subdirectory(src)

if(SDL_WEBGPU_BUILD_DEMO)
    add_subdirectory(demo)
endif()
