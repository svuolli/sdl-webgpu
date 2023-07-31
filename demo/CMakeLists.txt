
add_executable(webgpu-demo)
target_sources(webgpu-demo PRIVATE main.cpp)
target_link_libraries(webgpu-demo PRIVATE SDL_webgpu)

if(WIN32)
    target_link_libraries(webgpu-demo PRIVATE SDL2::SDL2main)
    set_target_properties(webgpu-demo PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
