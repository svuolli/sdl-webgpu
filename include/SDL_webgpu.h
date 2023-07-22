#ifndef SDL_WEBGPU_H
#define SDL_WEBGPU_H

#include <webgpu/webgpu.h>
#include <SDL2/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

WGPUSurface SDL_Webgpu_CreateSurface(
    SDL_Window * window, WGPUInstance instance);

#ifdef __cplusplus
}
#endif

#endif /* SDL_WEBGPU_H */
