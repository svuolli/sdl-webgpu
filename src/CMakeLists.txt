add_library(SDL_webgpu STATIC)
target_sources(SDL_webgpu PRIVATE SDL_webgpu.c)
target_link_libraries(SDL_webgpu PUBLIC SDL2::SDL2 webgpu)
target_include_directories(SDL_webgpu PUBLIC "${CMAKE_SOURCE_DIR}/include")

if(APPLE)
    target_compile_options(SDL_webgpu PRIVATE -x objective-c)
    target_link_libraries(SDL_webgpu PUBLIC
        "-framework Cocoa"
        "-framework CoreVideo"
        "-framework IOKit"
        "-framework QuartzCore"
        "-framework AppKit")
endif()
