#include "SDL_webgpu.h"

#include <webgpu/webgpu.h>
#include <SDL2/SDL.h>

#include <array>
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

class frame_renderer
{
    public:
    frame_renderer(wgpu_app & app_instance) :
        m_app(app_instance)
    {
        m_shader_module =
            wgpuDeviceCreateShaderModule(m_app.wgpu_device, &shader_module_descriptor);

        if(!m_shader_module)
        {
            throw std::runtime_error{"Shader module creation failed"};
        }

        if(!validate_shader_module(m_shader_module))
        {
            throw std::runtime_error{"Shader compilation failed"};
        }

        WGPUColorTargetState const color_target =
        {
            .nextInChain = nullptr,
            .format = WGPUTextureFormat_BGRA8Unorm,
            .blend = nullptr,
            .writeMask = WGPUColorWriteMask_All
        };

        WGPUFragmentState const fragmet_state =
        {
            .nextInChain = nullptr,
            .module = m_shader_module,
            .entryPoint = "fs_main",
            .constantCount = 0u,
            .constants = nullptr,
            .targetCount = 1,
            .targets = &color_target
        };

        WGPUVertexAttribute const vertex_attrib =
        {
            .format = WGPUVertexFormat_Float32x2,
            .offset = 0,
            .shaderLocation = 0
        };

        WGPUVertexBufferLayout const buffer_layout =
        {
            .arrayStride = 2 * sizeof(float),
            .stepMode = WGPUVertexStepMode_Vertex,
            .attributeCount = 1,
            .attributes = &vertex_attrib
        };

        WGPURenderPipelineDescriptor const pipeline_descriptor =
        {
            .nextInChain = nullptr,
            .label = "RenderPipelineCCW",
            .layout = nullptr,
            .vertex =
            {
                .nextInChain = nullptr,
                .module = m_shader_module,
                .entryPoint = "vs_main",
                .constantCount = 0,
                .constants = nullptr,
                .bufferCount = 1,
                .buffers = &buffer_layout
            },
            .primitive =
            {
                .nextInChain = nullptr,
                .topology = WGPUPrimitiveTopology_TriangleList,
                .stripIndexFormat = WGPUIndexFormat_Undefined,
                .frontFace = WGPUFrontFace_CCW,
                .cullMode = WGPUCullMode_None
            },
            .depthStencil = nullptr,
            .multisample =
            {
                .nextInChain = nullptr,
                .count = 1,
                .mask = ~std::uint32_t{0},
                .alphaToCoverageEnabled = false
            },
            .fragment = &fragmet_state
        };

        m_ccw_pipeline = wgpuDeviceCreateRenderPipeline(
            m_app.wgpu_device, &pipeline_descriptor);

        if(!m_ccw_pipeline)
        {
            throw std::runtime_error{"RenderPipeline creation failed"};
        }

        m_vertices = wgpuDeviceCreateBuffer(m_app.wgpu_device, &vertex_buffer_descriptor);

        if(!m_vertices)
        {
            throw std::runtime_error{"Buffer creation failed"};
        }

        wgpuQueueWriteBuffer(
            m_app.wgpu_queue,
            m_vertices,
            0,
            vertex_data.data(),
            vertex_data.size() * sizeof(float));
    }

    ~frame_renderer()
    {
        wgpuBufferRelease(m_vertices);
        wgpuRenderPipelineRelease(m_ccw_pipeline);
        wgpuShaderModuleRelease(m_shader_module);
    }

    void render(WGPUTextureView next_texture)
    {
        WGPUCommandEncoder encoder =
            wgpuDeviceCreateCommandEncoder(m_app.wgpu_device, &command_encoder_descriptor);

        WGPURenderPassEncoder render_pass =
            createRenderPassEncoder(encoder, next_texture);

        wgpuRenderPassEncoderSetPipeline(render_pass, m_ccw_pipeline);
        wgpuRenderPassEncoderSetVertexBuffer(
            render_pass, 0, m_vertices, 0, vertex_data.size() * sizeof(float));
        wgpuRenderPassEncoderDraw(render_pass, 3, 1, 0, 0);
        wgpuRenderPassEncoderEnd(render_pass);

        WGPUCommandBuffer command_buffer =
            wgpuCommandEncoderFinish(encoder, &command_buffer_descriptor);

        wgpuQueueSubmit(m_app.wgpu_queue, 1, &command_buffer);

        wgpuCommandBufferRelease(command_buffer);
        wgpuRenderPassEncoderRelease(render_pass);
        wgpuCommandEncoderRelease(encoder);
    }

    private:
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

    static bool validate_shader_module(WGPUShaderModule module)
    {
        struct user_data_t
        {
            bool compilation_complete = false;
            bool compilation_success = false;
            std::mutex mutex;
            std::condition_variable cv;
        };

        user_data_t user_data;

        auto const shader_compile_callback = [](
            WGPUCompilationInfoRequestStatus status,
            WGPUCompilationInfo const * compilation_info,
            void * p_user_data)
        {
            auto * user_data = reinterpret_cast<user_data_t *>(p_user_data);
            std::unique_lock<std::mutex> lock{user_data->mutex};            

            auto errors = std::size_t{0};
            if(compilation_info)
            {
                for(auto i = std::size_t{0}; i < compilation_info->messageCount; ++i)
                {
                    auto const message = compilation_info->messages[i];
                    if(message.type == WGPUCompilationMessageType_Error)
                    {
                        ++errors;
                        std::cerr << "Shader compile error: " << message.message << '\n';
                    }
                }
            }

            user_data->compilation_success = 
                (status == WGPUCompilationInfoRequestStatus_Success) &&
                (errors == 0u);
            user_data->compilation_complete = true;
            lock.unlock();
            user_data->cv.notify_all();
        };

        wgpuShaderModuleGetCompilationInfo(
            module, shader_compile_callback, &user_data);
        
        std::unique_lock<std::mutex> lock{user_data.mutex};
        user_data.cv.wait(
            lock, [&user_data] { return user_data.compilation_complete; });

        return user_data.compilation_success;
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

    static constexpr WGPUColor bg_color =
        {.06274509803921568627, .06274509803921568627, .19215686274509803921, 1.0};

    static constexpr WGPUShaderModuleWGSLDescriptor shader_code_descriptor =
    {
        .chain =
        { 
            .next = nullptr,
            .sType = WGPUSType_ShaderModuleWGSLDescriptor
        },
        .code = R"WGSL(
@vertex
fn vs_main(@location(0) vertex_pos: vec2f) -> @builtin(position) vec4f {
    return vec4f(vertex_pos, 0.0, 1.0);
}

@fragment
fn fs_main() -> @location(0) vec4f {
    return vec4f(0.0, 0.4, 1.0, 1.0);
}
        )WGSL"
    };

    static constexpr WGPUShaderModuleDescriptor shader_module_descriptor =
    {
        .nextInChain = &shader_code_descriptor.chain,
        .label = "ShaderModule"
    };

    static constexpr std::array<float, 3*2> vertex_data =
    {
        -0.5f, -0.5f,
        0.5f, -0.5f,
        0.0f, 0.5f,
    };

    static constexpr WGPUBufferDescriptor vertex_buffer_descriptor =
    {
        .nextInChain = nullptr,
        .label = "VertexBuffer",
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
        .size = vertex_data.size() * sizeof(float),
        .mappedAtCreation = false
    };

    wgpu_app & m_app;
    WGPUShaderModule m_shader_module = nullptr;
    WGPURenderPipeline m_ccw_pipeline = nullptr;
    WGPUBuffer m_vertices;
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
