#include "SDL_webgpu.h"

#include <webgpu/webgpu.h>
#include <SDL2/SDL.h>

#include <condition_variable>
#include <mutex>
#include <iostream>
#include <string>
#include <stdexcept>
#include <thread>

struct wgpu_app
{
    wgpu_app()
    {
        if(SDL_VideoInit(nullptr))
        {
            throw std::runtime_error{"SDL_VideoInit failed"};
        }

        WGPUInstanceDescriptor const instance_descriptor = { nullptr };
        wgpu_instance = wgpuCreateInstance(&instance_descriptor);

        if(!wgpu_instance)
        {
            throw std::runtime_error{"wgpuCreateInstance failed"};
        }

        sdl_window = SDL_CreateWindow(
            "SDL_wgpu Demo",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            width, height,
            0);

        if(!sdl_window)
        {
            throw std::runtime_error{"SDL_CreateWindow failed"};
        }

        wgpu_surface = SDL_Webgpu_CreateSurface(sdl_window, wgpu_instance);

        if(!wgpu_surface)
        {
            throw std::runtime_error{"SDL_Webgpu_CreateSurface failed"};
        }

        WGPURequestAdapterOptions const adapter_options = {
            .nextInChain = nullptr,
            .compatibleSurface = wgpu_surface,
            .powerPreference = WGPUPowerPreference_Undefined,
            .backendType = WGPUBackendType_Undefined,
            .forceFallbackAdapter = false,
            .compatibilityMode = false,
        };

        wgpu_adapter = requestAdapter(wgpu_instance, adapter_options);

        if(!wgpu_adapter)
        {
            throw std::runtime_error{"wgpuInstanceRequestAdapter failed"};
        }

        WGPUDeviceDescriptor const device_descriptor = {
            .nextInChain = nullptr,
            .label = "Device",
            .requiredFeaturesCount = 0u,
            .requiredFeatures = nullptr,
            .requiredLimits = nullptr,
            .defaultQueue = { .nextInChain = nullptr, .label = "Queue" },
            .deviceLostCallback = nullptr,
            .deviceLostUserdata = nullptr
        };

        wgpu_device = requestDevice(wgpu_adapter, device_descriptor);

        if(!wgpu_device)
        {
            throw std::runtime_error{"wgpuAdapterRequestDevice failed"};
        }

        wgpuDeviceSetDeviceLostCallback(wgpu_device, nullptr, nullptr);

        wgpu_queue = wgpuDeviceGetQueue(wgpu_device);

        if(!wgpu_queue)
        {
            throw std::runtime_error{"WGPU Device has no command queue"};
        }

        WGPUSwapChainDescriptor const swap_chain_descriptor = {
            .nextInChain = nullptr,
            .label = "SwapChain",
            .usage = WGPUTextureUsage_RenderAttachment,
            .format = WGPUTextureFormat_BGRA8Unorm,
            .width = width,
            .height = height,
            .presentMode = WGPUPresentMode_Fifo
        };

        wgpu_swap_chain = wgpuDeviceCreateSwapChain(
            wgpu_device, wgpu_surface, &swap_chain_descriptor);
    }

    ~wgpu_app()
    {
        wgpuSwapChainRelease(wgpu_swap_chain);
        wgpuQueueRelease(wgpu_queue);
        wgpuDeviceRelease(wgpu_device);
        wgpuSurfaceRelease(wgpu_surface);
        wgpuAdapterRelease(wgpu_adapter);
        SDL_DestroyWindow(sdl_window);
        wgpuInstanceRelease(wgpu_instance);
        SDL_VideoQuit();
    }

    wgpu_app(wgpu_app const &) = delete;

    wgpu_app & operator=(wgpu_app const &) = delete;
    wgpu_app & operator=(wgpu_app &&) = delete;

    static constexpr int width = 800;
    static constexpr int height = 600;

    static WGPUAdapter requestAdapter(
        WGPUInstance instance, WGPURequestAdapterOptions const & opts)
    {
        struct user_data_t
        {
            WGPUAdapter adapter = nullptr;
            bool request_complete = false;
            std::mutex mutex;
            std::condition_variable cv;
        };

        user_data_t user_data;
        
        auto const request_callback = [](
            WGPURequestAdapterStatus status,
            WGPUAdapter adapter,
            char const * message,
            void * p_user_data)
        {
            user_data_t * user_data =
                reinterpret_cast<user_data_t *>(p_user_data);
            std::unique_lock<std::mutex> lock{user_data->mutex};

            if(status == WGPURequestAdapterStatus_Success)
            {
                user_data->adapter = adapter;
            }

            user_data->request_complete = true;
            lock.unlock();
            user_data->cv.notify_all();
        };

        wgpuInstanceRequestAdapter(
            instance, &opts, request_callback, &user_data);

        std::unique_lock<std::mutex> lock{user_data.mutex};
        user_data.cv.wait(lock, [&user_data] { return user_data.request_complete; });

        return user_data.adapter;
    }

    static WGPUDevice requestDevice(
        WGPUAdapter adapter, WGPUDeviceDescriptor const & desc)
    {
        struct user_data_t
        {
            WGPUDevice device = nullptr;
            bool request_complete = false;
            std::mutex mutex;
            std::condition_variable cv;
        };

        user_data_t user_data;

        auto const request_callback = [](
            WGPURequestDeviceStatus status,
            WGPUDevice device,
            char const * message,
            void * p_user_data)
        {
            user_data_t * user_data = reinterpret_cast<user_data_t *>(p_user_data);
            std::unique_lock<std::mutex> lock{user_data->mutex};

            if(status == WGPURequestDeviceStatus_Success)
            {
                user_data->device = device;
            }

            user_data->request_complete = true;
            lock.unlock();
            user_data->cv.notify_all();
        };

        wgpuAdapterRequestDevice(
            adapter, &desc, request_callback, &user_data);

        std::unique_lock<std::mutex> lock{user_data.mutex};
        user_data.cv.wait(lock, [&user_data] { return user_data.request_complete; });

        return user_data.device;
    }

    WGPUInstance wgpu_instance = nullptr;
    SDL_Window * sdl_window = nullptr;
    WGPUAdapter wgpu_adapter = nullptr;
    WGPUSurface wgpu_surface = nullptr;
    WGPUDevice wgpu_device = nullptr;
    WGPUQueue wgpu_queue = nullptr;
    WGPUSwapChain wgpu_swap_chain = nullptr;
};

int main(int argc, char const * argv[])
{
    try
    {
        wgpu_app app;

        auto done = false;

        while(!done)
        {
            auto event = SDL_Event{};
            while(SDL_PollEvent(&event))
            {
                if(event.type == SDL_QUIT)
                {
                    done = true;
                }
            }
        }

    }
    catch (std::exception const & e)
    {
        std::cerr << "Exception caught: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
