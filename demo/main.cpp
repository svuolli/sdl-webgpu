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

        WGPURequestAdapterOptions const adapter_options =
        {
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

        WGPUDeviceDescriptor const device_descriptor =
        {
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
        wgpuDeviceSetUncapturedErrorCallback(
            wgpu_device, &wgpu_app::wgpu_error_callback, this);

        wgpu_queue = wgpuDeviceGetQueue(wgpu_device);

        if(!wgpu_queue)
        {
            throw std::runtime_error{"WGPU Device has no command queue"};
        }

        WGPUSwapChainDescriptor const swap_chain_descriptor =
        {
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

    static void wgpu_error_callback(
        WGPUErrorType type, char const * message, void * user_data)
    {
        (void)user_data;
        std::cerr <<
            "Uncaught WGPU error(" << static_cast<int>(type) << "): " <<
            message << std::endl;
    }

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

    static constexpr int width = 800;
    static constexpr int height = 600;

    WGPUInstance wgpu_instance = nullptr;
    SDL_Window * sdl_window = nullptr;
    WGPUAdapter wgpu_adapter = nullptr;
    WGPUSurface wgpu_surface = nullptr;
    WGPUDevice wgpu_device = nullptr;
    WGPUQueue wgpu_queue = nullptr;
    WGPUSwapChain wgpu_swap_chain = nullptr;
};

struct frame_renderer
{
    frame_renderer(wgpu_app & app_instance) :
        app(app_instance)
    {}

    ~frame_renderer()
    {}

    void render(WGPUTextureView next_texture)
    {
        WGPUCommandEncoder encoder =
            wgpuDeviceCreateCommandEncoder(app.wgpu_device, &command_encoder_descriptor);

        WGPURenderPassEncoder render_pass = createRenderPassEncoder(encoder, next_texture);

        // TODO: Add render commands

        wgpuRenderPassEncoderEnd(render_pass);
        WGPUCommandBuffer command_buffer =
            wgpuCommandEncoderFinish(encoder, &command_buffer_descriptor);

        wgpuQueueSubmit(app.wgpu_queue, 1, &command_buffer);

        wgpuCommandBufferRelease(command_buffer);
        wgpuRenderPassEncoderRelease(render_pass);
        wgpuCommandEncoderRelease(encoder);
    }

    WGPURenderPassEncoder createRenderPassEncoder(
        WGPUCommandEncoder encoder, WGPUTextureView target_view)
    {
        WGPURenderPassColorAttachment const color_attachment =
        {
            .nextInChain = nullptr,
            .view = target_view,
            .resolveTarget = nullptr,
            .loadOp = WGPULoadOp_Clear,
            .storeOp = WGPUStoreOp_Store,
            .clearValue = bg_color
        };

        WGPURenderPassDescriptor const render_pass_descriptor =
        {
            .nextInChain = nullptr,
            .label = "RenderPass",
            .colorAttachmentCount = 1,
            .colorAttachments = &color_attachment,
            .depthStencilAttachment = nullptr,
            .occlusionQuerySet = nullptr,
            .timestampWriteCount = 0,
            .timestampWrites = nullptr
        };

        return wgpuCommandEncoderBeginRenderPass(encoder, &render_pass_descriptor);
    }

    static constexpr WGPUCommandEncoderDescriptor command_encoder_descriptor =
    {
        .nextInChain = nullptr,
        .label = "CommandEncoder"
    };

    static constexpr WGPUCommandBufferDescriptor command_buffer_descriptor =
    {
        .nextInChain = nullptr,
        .label = "CommandBuffer"
    };

    static constexpr WGPUColor bg_color = {0.8, 0.2, 0.6, 1.0};

    wgpu_app & app;
};

int main(int argc, char const * argv[])
{
    try
    {
        wgpu_app app;
        frame_renderer renderer(app);

        auto done = false;
        auto const begin_time = SDL_GetTicks();
        auto prev_time = begin_time;
        auto frame_count = std::size_t{0};

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

            auto const current_time = SDL_GetTicks();
            auto const delta_time = current_time - prev_time;
            auto const elapsed_time = current_time - begin_time;

            WGPUTextureView next_texture =
                wgpuSwapChainGetCurrentTextureView(app.wgpu_swap_chain);

            if(!next_texture)
            {
                std::cerr <<
                    "Retrieving next texture view from swap chain failed\n";
                continue;
            }

            renderer.render(next_texture);

            wgpuTextureViewRelease(next_texture);
            wgpuSwapChainPresent(app.wgpu_swap_chain);

            prev_time = current_time;
            ++frame_count;
        }
        auto const elapsed_time = prev_time - begin_time;
        std::cout << frame_count << " frames in " << elapsed_time << "ms\n";
        auto const frame_rate =
            1000.0 * static_cast<double>(frame_count) /
            static_cast<double>(elapsed_time);
        std::cout << frame_rate << "Hz\n";

    }
    catch (std::exception const & e)
    {
        std::cerr << "Exception caught: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
