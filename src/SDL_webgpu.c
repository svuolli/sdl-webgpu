#include "SDL_webgpu.h"
#include <SDL2/SDL_syswm.h>

#if defined(SDL_VIDEO_DRIVER_X11)
static WGPUSurface SDL_Webgpu_CreateSurface_X11(
    SDL_SysWMinfo * wminfo, WGPUInstance instance)
{
    WGPUSurfaceDescriptorFromXlibWindow const x11_surface_descriptor = {
        .chain = { .next = NULL, .sType = WGPUSType_SurfaceDescriptorFromXlibWindow },
        .display = wminfo->info.x11.display,
        .window = wminfo->info.x11.window,
    };

    WGPUSurfaceDescriptor const surface_descriptor = {
        .label = NULL,
        .nextInChain = (WGPUChainedStruct const *)&x11_surface_descriptor,
    };

    return wgpuInstanceCreateSurface(instance, &surface_descriptor);
}
#endif

#if defined(SDL_VIDEO_DRIVER_WAYLAND)
static WGPUSurface SDL_Webgpu_CreateSurface_Wayland(
    SDL_SysWMinfo * wminfo, WGPUInstance instance)
{
    WGPUSurfaceDescriptorFromWaylandSurface const wl_surface_descriptor = {
        .chain = { .next = NULL, .sType = WGPUSType_SurfaceDescriptorFromWaylandSurface },
        .surface = wminfo->info.wl.surface,
        .display = wminfo->info.wl.display
    };

    WGPUSurfaceDescriptor const surface_descriptor = {
        .label = NULL,
        .nextInChain = (WGPUChainedStruct const *)&wl_surface_descriptor,
    };

    return wgpuInstanceCreateSurface(instance, &surface_descriptor);
}
#endif

WGPUSurface SDL_Webgpu_CreateSurface(
    SDL_Window * window, WGPUInstance instance)
{
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);

    SDL_GetWindowWMInfo(window, &info);

    switch(info.subsystem)
    {
#if defined(SDL_VIDEO_DRIVER_X11)
        case SDL_SYSWM_X11:
            return SDL_Webgpu_CreateSurface_X11(&info, instance);
#endif

#if defined(SDL_VIDEO_DRIVER_X11)
        case SDL_SYSWM_WAYLAND:
            return SDL_Webgpu_CreateSurface_Wayland(&info, instance);
#endif

        default:
            return NULL;
    }
}
