
add_executable(webgpu-demo)
target_sources(webgpu-demo PRIVATE main.cpp)
target_link_libraries(webgpu-demo PRIVATE SDL_webgpu)
set_target_properties(
    webgpu-demo PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON)

if(WIN32)
    target_link_libraries(webgpu-demo PRIVATE SDL2::SDL2main)
    set_target_properties(webgpu-demo PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
