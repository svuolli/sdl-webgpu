#include "SDL_webgpu.h"
#include <SDL2/SDL_syswm.h>

#if defined(SDL_VIDEO_DRIVER_WINDOWS)
static WGPUSurface SDL_Webgpu_CreateSurface_Win32(
    SDL_SysWMinfo * wminfo, WGPUInstance instance)
{
    WGPUSurfaceDescriptorFromWindowsHWND const win32_surface_descriptor = {
        .chain = { .next = NULL, .sType = WGPUSType_SurfaceDescriptorFromWindowsHWND },
        .hinstance = wminfo->info.win.hinstance,
        .hwnd = wminfo->info.win.window,
    };

    WGPUSurfaceDescriptor const surface_descriptor = {
        .label = NULL,
        .nextInChain = &win32_surface_descriptor.chain,
    };

    return wgpuInstanceCreateSurface(instance, &surface_descriptor);

    return NULL;
}
#endif

#if defined(SDL_VIDEO_DRIVER_COCOA)
#include <Foundation/Foundation.h>
#include <QuartzCore/CAMetalLayer.h>
#include <AppKit/NSWindow.h>

static WGPUSurface SDL_Webgpu_CreateSurface_Cocoa(
    SDL_SysWMinfo * wminfo, WGPUInstance instance)
{
    id metal_layer = [CAMetalLayer layer];
    NSWindow * window = wminfo->info.cocoa.window;
    [window.contentView setWantsLayer: YES];
    [window.contentView setLayer: metal_layer];

    WGPUSurfaceDescriptorFromMetalLayer const metal_surface_descriptor = {
        .chain = { .next = NULL, .sType = WGPUSType_SurfaceDescriptorFromMetalLayer },
        .layer = metal_layer,
    };

    WGPUSurfaceDescriptor const surface_descriptor = {
        .label = NULL,
        .nextInChain = &metal_surface_descriptor.chain,
    };

    return wgpuInstanceCreateSurface(instance, &surface_descriptor);
}
#endif

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
        .nextInChain = &x11_surface_descriptor.chain,
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
        .nextInChain = &wl_surface_descriptor.chain,
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

#if defined(SDL_VIDEO_DRIVER_WAYLAND)
        case SDL_SYSWM_WAYLAND:
            return SDL_Webgpu_CreateSurface_Wayland(&info, instance);
#endif

#if defined(SDL_VIDEO_DRIVER_COCOA)
        case SDL_SYSWM_COCOA:
            return SDL_Webgpu_CreateSurface_Cocoa(&info, instance);
#endif

#if defined(SDL_VIDEO_DRIVER_WINDOWS)
        case SDL_SYSWM_COCOA:
            return SDL_Webgpu_CreateSurface_Win32(&info, instance);
#endif

        default:
            return NULL;
    }
}
