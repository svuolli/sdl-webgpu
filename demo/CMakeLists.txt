
add_executable(webgpu-demo)
target_sources(webgpu-demo PRIVATE main.cpp)
target_link_libraries(webgpu-demo PRIVATE SDL_webgpu)
