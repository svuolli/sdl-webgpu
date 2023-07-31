#include "SDL_webgpu.h"
#include <SDL2/SDL_main.h>

#include <webgpu/webgpu.h>
#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

#include <array>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <string>
#include <stdexcept>
#include <thread>
#include <vector>

namespace
{
static constexpr WGPUColor int_to_wgpu_color(std::uint32_t c)
{
    auto const conv = [c](std::size_t comp)
    {
        return ((c >> (8*comp)) & 0xFF) / 255.0f;
    };

    return { conv(2), conv(1), conv(0), conv(3) };
}

static constexpr glm::vec4 int_to_glm_color(std::uint32_t c)
{
    auto const conv = [c](std::size_t comp)
    {
        return ((c >> (8*comp)) & 0xFF) / 255.0f;
    };

    return { conv(2), conv(1), conv(0), conv(3) };
}

std::ostream & operator<<(std::ostream & stream, WGPULimits const & limits)
{
#define WEBGPU_SDL_STREAM_ONE_LIMIT_ITEM(item) (stream << " - " #item << ": " << limits.item << '\n')

    WEBGPU_SDL_STREAM_ONE_LIMIT_ITEM(maxTextureDimension1D);
    WEBGPU_SDL_STREAM_ONE_LIMIT_ITEM(maxTextureDimension2D);
    WEBGPU_SDL_STREAM_ONE_LIMIT_ITEM(maxTextureDimension3D);
    WEBGPU_SDL_STREAM_ONE_LIMIT_ITEM(maxTextureArrayLayers);
    WEBGPU_SDL_STREAM_ONE_LIMIT_ITEM(maxBindGroups);
    WEBGPU_SDL_STREAM_ONE_LIMIT_ITEM(maxBindGroupsPlusVertexBuffers);
    WEBGPU_SDL_STREAM_ONE_LIMIT_ITEM(maxBindingsPerBindGroup);
    WEBGPU_SDL_STREAM_ONE_LIMIT_ITEM(maxDynamicUniformBuffersPerPipelineLayout);
    WEBGPU_SDL_STREAM_ONE_LIMIT_ITEM(maxDynamicStorageBuffersPerPipelineLayout);
    WEBGPU_SDL_STREAM_ONE_LIMIT_ITEM(maxSampledTexturesPerShaderStage);
    WEBGPU_SDL_STREAM_ONE_LIMIT_ITEM(maxSamplersPerShaderStage);
    WEBGPU_SDL_STREAM_ONE_LIMIT_ITEM(maxStorageBuffersPerShaderStage);
    WEBGPU_SDL_STREAM_ONE_LIMIT_ITEM(maxStorageTexturesPerShaderStage);
    WEBGPU_SDL_STREAM_ONE_LIMIT_ITEM(maxUniformBuffersPerShaderStage);
    WEBGPU_SDL_STREAM_ONE_LIMIT_ITEM(maxUniformBufferBindingSize);
    WEBGPU_SDL_STREAM_ONE_LIMIT_ITEM(maxStorageBufferBindingSize);
    WEBGPU_SDL_STREAM_ONE_LIMIT_ITEM(minUniformBufferOffsetAlignment);
    WEBGPU_SDL_STREAM_ONE_LIMIT_ITEM(minStorageBufferOffsetAlignment);
    WEBGPU_SDL_STREAM_ONE_LIMIT_ITEM(maxVertexBuffers);
    WEBGPU_SDL_STREAM_ONE_LIMIT_ITEM(maxBufferSize);
    WEBGPU_SDL_STREAM_ONE_LIMIT_ITEM(maxVertexAttributes);
    WEBGPU_SDL_STREAM_ONE_LIMIT_ITEM(maxVertexBufferArrayStride);
    WEBGPU_SDL_STREAM_ONE_LIMIT_ITEM(maxInterStageShaderComponents);
    WEBGPU_SDL_STREAM_ONE_LIMIT_ITEM(maxInterStageShaderVariables);
    WEBGPU_SDL_STREAM_ONE_LIMIT_ITEM(maxColorAttachments);
    WEBGPU_SDL_STREAM_ONE_LIMIT_ITEM(maxColorAttachmentBytesPerSample);
    WEBGPU_SDL_STREAM_ONE_LIMIT_ITEM(maxComputeWorkgroupStorageSize);
    WEBGPU_SDL_STREAM_ONE_LIMIT_ITEM(maxComputeInvocationsPerWorkgroup);
    WEBGPU_SDL_STREAM_ONE_LIMIT_ITEM(maxComputeWorkgroupSizeX);
    WEBGPU_SDL_STREAM_ONE_LIMIT_ITEM(maxComputeWorkgroupSizeY);
    WEBGPU_SDL_STREAM_ONE_LIMIT_ITEM(maxComputeWorkgroupSizeZ);
    WEBGPU_SDL_STREAM_ONE_LIMIT_ITEM(maxComputeWorkgroupsPerDimension);

#undef WEBGPU_SDL_STREAM_ONE_LIMIT_ITEM

    return stream;
}

} /* namespace */

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
            .requiredLimits = &required_device_limits,
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

    static constexpr float aspect_ratio()
    {
        return static_cast<float>(width) / static_cast<float>(height);
    }

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

    static constexpr auto uniform_buffer_offset_alignment = 256u;
    static constexpr WGPURequiredLimits required_device_limits
    {
        .nextInChain = nullptr,
        .limits =
        {
            .maxTextureDimension1D = 0,
            .maxTextureDimension2D = 0,
            .maxTextureDimension3D = 0,
            .maxTextureArrayLayers = 0,
            .maxBindGroups = 1,
            .maxBindGroupsPlusVertexBuffers = 4,
            .maxBindingsPerBindGroup = 4,
            .maxDynamicUniformBuffersPerPipelineLayout = 2,
            .maxDynamicStorageBuffersPerPipelineLayout = 0,
            .maxSampledTexturesPerShaderStage = 0,
            .maxSamplersPerShaderStage = 0,
            .maxStorageBuffersPerShaderStage = 0,
            .maxStorageTexturesPerShaderStage = 0,
            .maxUniformBuffersPerShaderStage = 4,
            .maxUniformBufferBindingSize = 4096,
            .maxStorageBufferBindingSize = 0,
            .minUniformBufferOffsetAlignment = uniform_buffer_offset_alignment,
            .minStorageBufferOffsetAlignment = 1024,
            .maxVertexBuffers = 4,
            .maxBufferSize = 1024*1024,
            .maxVertexAttributes = 4,
            .maxVertexBufferArrayStride = 512,
            .maxInterStageShaderComponents = 16,
            .maxInterStageShaderVariables = 8,
            .maxColorAttachments = 1,
            .maxColorAttachmentBytesPerSample = 16,
            .maxComputeWorkgroupStorageSize = 0,
            .maxComputeInvocationsPerWorkgroup = 0,
            .maxComputeWorkgroupSizeX = 0,
            .maxComputeWorkgroupSizeY = 0,
            .maxComputeWorkgroupSizeZ = 0,
            .maxComputeWorkgroupsPerDimension = 0,
        }
    };

    static constexpr int width = 800;
    static constexpr int height = 600;

    WGPUInstance wgpu_instance = nullptr;
    SDL_Window * sdl_window = nullptr;
    WGPUAdapter wgpu_adapter = nullptr;
    WGPUSurface wgpu_surface = nullptr;
    WGPUDevice wgpu_device = nullptr;
    WGPUQueue wgpu_queue = nullptr;
    WGPUSwapChain wgpu_swap_chain = nullptr;
}; /* struct wgpu_app */

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

        constexpr std::array src_vertex_attribs
        {
            WGPUVertexAttribute
            {
                .format = WGPUVertexFormat_Float32x3,
                .offset = 0,
                .shaderLocation = 0
            },
        };

        constexpr std::array dst_vertex_attribs
        {
            WGPUVertexAttribute
            {
                .format = WGPUVertexFormat_Float32x3,
                .offset = 0,
                .shaderLocation = 1
            },
        };

        std::array const buffer_layouts
        {
            WGPUVertexBufferLayout
            {
                .arrayStride = 3 * sizeof(float),
                .stepMode = WGPUVertexStepMode_Vertex,
                .attributeCount = src_vertex_attribs.size(),
                .attributes = src_vertex_attribs.data()
            },
            WGPUVertexBufferLayout
            {
                .arrayStride = 3 * sizeof(float),
                .stepMode = WGPUVertexStepMode_Vertex,
                .attributeCount = dst_vertex_attribs.size(),
                .attributes = dst_vertex_attribs.data()
            }
        };

        WGPUBindGroupLayoutDescriptor const bind_group_layout_descriptor =
        {
            .nextInChain = nullptr,
            .label = "BindGrouLayout",
            .entryCount = binding_layout_entries.size(),
            .entries = binding_layout_entries.data()
        };

        WGPUBindGroupLayout bind_group_layout =
            wgpuDeviceCreateBindGroupLayout(
                m_app.wgpu_device, &bind_group_layout_descriptor);

        WGPUPipelineLayoutDescriptor const layout_descriptor =
        {
            .nextInChain = nullptr,
            .label = "PipelineLayout",
            .bindGroupLayoutCount = 1,
            .bindGroupLayouts = &bind_group_layout
        };

        WGPUPipelineLayout pipeline_layout =
            wgpuDeviceCreatePipelineLayout(m_app.wgpu_device, &layout_descriptor);

        WGPURenderPipelineDescriptor const front_face_pipeline_descriptor =
        {
            .nextInChain = nullptr,
            .label = "RenderPipelineCCW",
            .layout = pipeline_layout,
            .vertex =
            {
                .nextInChain = nullptr,
                .module = m_shader_module,
                .entryPoint = "vs_main",
                .constantCount = 0,
                .constants = nullptr,
                .bufferCount = buffer_layouts.size(),
                .buffers = buffer_layouts.data()
            },
            .primitive =
            {
                .nextInChain = nullptr,
                .topology = WGPUPrimitiveTopology_TriangleList,
                .stripIndexFormat = WGPUIndexFormat_Undefined,
                .frontFace = WGPUFrontFace_CCW,
                .cullMode = WGPUCullMode_Front
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

        m_front_face_pipeline = wgpuDeviceCreateRenderPipeline(
            m_app.wgpu_device, &front_face_pipeline_descriptor);

        if(!m_front_face_pipeline)
        {
            throw std::runtime_error{"RenderPipeline creation failed"};
        }

        WGPURenderPipelineDescriptor const back_face_pipeline_descriptor =
        {
            .nextInChain = nullptr,
            .label = "RenderPipelineCW",
            .layout = pipeline_layout,
            .vertex =
            {
                .nextInChain = nullptr,
                .module = m_shader_module,
                .entryPoint = "vs_main",
                .constantCount = 0,
                .constants = nullptr,
                .bufferCount = buffer_layouts.size(),
                .buffers = buffer_layouts.data()
            },
            .primitive =
            {
                .nextInChain = nullptr,
                .topology = WGPUPrimitiveTopology_TriangleList,
                .stripIndexFormat = WGPUIndexFormat_Undefined,
                .frontFace = WGPUFrontFace_CCW,
                .cullMode = WGPUCullMode_Back
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

        m_back_face_pipeline = wgpuDeviceCreateRenderPipeline(
            m_app.wgpu_device, &back_face_pipeline_descriptor);

        if(!m_back_face_pipeline)
        {
            throw std::runtime_error{"RenderPipeline creation failed"};
        }

        wgpuPipelineLayoutRelease(pipeline_layout);

        m_shape_vertex_buffers =
        {
            create_vertex_buffer(cube_vertex_data, "CubeVertexBuffer"),
            create_vertex_buffer(hedron_vertex_data, "HedronVertexBuffer"),
            create_vertex_buffer(spikes_vertex_data, "SpikesVertexBuffer"),
            create_vertex_buffer(tile1_vertex_data, "Tile1VertexBuffer"),
            create_vertex_buffer(tile2_vertex_data, "Tile2VertexBuffer")
        };

        m_indices1 = create_index_buffer(indices1_data, "IndexBuffer1");
        m_indices2 = create_index_buffer(indices2_data, "IndexBuffer2");
        m_transformation_uniform =
            create_uniform_buffer(sizeof(glm::mat4) * 2, "TransformationUniform");
        m_color_uniform = create_uniform_buffer(
            fill_colors.size() * wgpu_app::uniform_buffer_offset_alignment, "ColorUniform");

        for(auto i = std::size_t{0}; i != fill_colors.size(); ++i)
        {
            auto const & color = fill_colors[i];
            wgpuQueueWriteBuffer(
                m_app.wgpu_queue,
                m_color_uniform,
                i*wgpu_app::uniform_buffer_offset_alignment,
                &color, sizeof(color));
        }

        wgpuQueueWriteBuffer(
            m_app.wgpu_queue,
            m_color_uniform,
            0,
            fill_colors.data(),
            fill_colors.size() * sizeof(WGPUColor));

        std::array const bind_group_entries
        {
            WGPUBindGroupEntry
            {
                .nextInChain = nullptr,
                .binding = 0,
                .buffer = m_transformation_uniform,
                .offset = 0,
                .size = sizeof(glm::mat4) * 2,
                .sampler = nullptr,
                .textureView = nullptr
            },
            WGPUBindGroupEntry
            {
                .nextInChain = nullptr,
                .binding = 1,
                .buffer = m_color_uniform,
                .offset = 0,
                .size = sizeof(WGPUColor),
                .sampler = nullptr,
                .textureView = nullptr
            }
        };

        WGPUBindGroupDescriptor const bind_group_descriptor =
        {
            .nextInChain = nullptr,
            .label = "BindGroup",
            .layout = bind_group_layout,
            .entryCount = bind_group_entries.size(),
            .entries = bind_group_entries.data()
        };

        m_bind_group = wgpuDeviceCreateBindGroup(
            m_app.wgpu_device, &bind_group_descriptor);

        m_projection_matrix =
            glm::perspective(45.0f, wgpu_app::aspect_ratio(), 1.0f, 50.0f);

        wgpuBindGroupLayoutRelease(bind_group_layout);
    }

    ~frame_renderer()
    {
        wgpuBufferRelease(m_color_uniform);
        wgpuBufferRelease(m_transformation_uniform);
        wgpuBufferRelease(m_indices2);
        wgpuBufferRelease(m_indices1);

        for(auto buff: m_shape_vertex_buffers)
        {
            wgpuBufferRelease(buff);
        }

        wgpuRenderPipelineRelease(m_back_face_pipeline);
        wgpuRenderPipelineRelease(m_front_face_pipeline);
        wgpuShaderModuleRelease(m_shader_module);
    }

    void render(
        WGPUTextureView next_texture,
        std::uint32_t time_point,
        std::uint32_t delta_time)
    {
        float rc = 3.0f * glm::cos(time_point * 0.001);
        float sc = 2.5f * glm::sin(time_point * 0.001);

        glm::mat4 transform =
            m_projection_matrix *
            glm::translate(glm::vec3{0.0f, 0.0f, -8.0f}) *
            glm::rotate(rc, glm::vec3{1.0f, 0.0f, 0.0f}) *
            glm::rotate(rc, glm::vec3{0.0f, 1.0f, 0.0f});

        wgpuQueueWriteBuffer(
            m_app.wgpu_queue,
            m_transformation_uniform,
            0,
            &transform,
            sizeof(transform));

        while(m_morph_time > 1.0f)
        {
            m_morph_time -= 1.0f;
            ++m_morph_index;
        }

        auto const morph_time =
            glm::clamp((m_morph_time * 4.0f) - 3.0f, 0.0f, 1.0f);

        wgpuQueueWriteBuffer(
            m_app.wgpu_queue,
            m_transformation_uniform,
            sizeof(transform),
            &morph_time,
            sizeof(morph_time));

        auto const src_index = m_morph_index % m_shape_vertex_buffers.size();
        auto const dst_index = (src_index+1) % m_shape_vertex_buffers.size();

        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(
            m_app.wgpu_device, &command_encoder_descriptor);

        WGPURenderPassEncoder render_pass =
            createRenderPassEncoder(encoder, next_texture);

        wgpuRenderPassEncoderSetVertexBuffer(
            render_pass, 0, m_shape_vertex_buffers[src_index],
            0, cube_vertex_data.size() * sizeof(glm::vec3));
        wgpuRenderPassEncoderSetVertexBuffer(
            render_pass, 1, m_shape_vertex_buffers[dst_index],
            0, hedron_vertex_data.size() * sizeof(glm::vec3));

        // 1st draww
        wgpuRenderPassEncoderSetPipeline(render_pass, m_front_face_pipeline);

        std::uint32_t color_offset =
            wgpu_app::uniform_buffer_offset_alignment * 0;
        wgpuRenderPassEncoderSetBindGroup(
            render_pass, 0, m_bind_group, 1, &color_offset);

        wgpuRenderPassEncoderSetIndexBuffer(
            render_pass,
            m_indices1,
            WGPUIndexFormat_Uint32,
            0, indices1_data.size() * sizeof(std::uint32_t));

        wgpuRenderPassEncoderDrawIndexed(
            render_pass, indices1_data.size(), 1, 0, 0, 0);

        // 2nd draw
        color_offset =
            wgpu_app::uniform_buffer_offset_alignment * 1;
        wgpuRenderPassEncoderSetBindGroup(
            render_pass, 0, m_bind_group, 1, &color_offset);

        wgpuRenderPassEncoderSetIndexBuffer(
            render_pass,
            m_indices2,
            WGPUIndexFormat_Uint32,
            0, indices2_data.size() * sizeof(std::uint32_t));

        wgpuRenderPassEncoderDrawIndexed(
            render_pass, indices2_data.size(), 1, 0, 0, 0);

        // 3rd draw
        wgpuRenderPassEncoderSetPipeline(render_pass, m_back_face_pipeline);

        color_offset =
            wgpu_app::uniform_buffer_offset_alignment * 2;
        wgpuRenderPassEncoderSetBindGroup(
            render_pass, 0, m_bind_group, 1, &color_offset);

        wgpuRenderPassEncoderSetIndexBuffer(
            render_pass,
            m_indices2,
            WGPUIndexFormat_Uint32,
            0, indices2_data.size() * sizeof(std::uint32_t));

        wgpuRenderPassEncoderDrawIndexed(
            render_pass, indices2_data.size(), 1, 0, 0, 0);

        // Draw done

        wgpuRenderPassEncoderEnd(render_pass);

        WGPUCommandBuffer command_buffer =
            wgpuCommandEncoderFinish(encoder, &command_buffer_descriptor);

        wgpuQueueSubmit(m_app.wgpu_queue, 1, &command_buffer);

        wgpuCommandBufferRelease(command_buffer);
        wgpuRenderPassEncoderRelease(render_pass);
        wgpuCommandEncoderRelease(encoder);

        m_morph_time += delta_time/3000.0f;
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

    template <typename Container>
    WGPUBuffer create_vertex_buffer(Container const & data, char const * label)
    {
        auto const size = data.size() * sizeof(typename Container::value_type);
        WGPUBufferDescriptor const descriptor =
        {
            .nextInChain = nullptr,
            .label = label,
            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
            .size = size,
            .mappedAtCreation = false
        };

        WGPUBuffer buffer =
            wgpuDeviceCreateBuffer(m_app.wgpu_device, &descriptor);

        wgpuQueueWriteBuffer(
            m_app.wgpu_queue,
            buffer,
            0,
            data.data(),
            data.size() * sizeof(typename Container::value_type));

        return buffer;
    }

    template <typename Container>
    WGPUBuffer create_index_buffer(Container const & data, char const * label)
    {
        auto const size = data.size() * sizeof(typename Container::value_type);
        WGPUBufferDescriptor const descriptor =
        {
            .nextInChain = nullptr,
            .label = label,
            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index,
            .size = size,
            .mappedAtCreation = false
        };

        WGPUBuffer buffer =
            wgpuDeviceCreateBuffer(m_app.wgpu_device, &descriptor);

        wgpuQueueWriteBuffer(
            m_app.wgpu_queue,
            buffer,
            0,
            data.data(),
            data.size() * sizeof(typename Container::value_type));

        return buffer;
    }

    WGPUBuffer create_uniform_buffer(std::size_t size, char const * label)
    {
        WGPUBufferDescriptor const descriptor =
        {
            .nextInChain = nullptr,
            .label = label,
            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
            .size = size,
            .mappedAtCreation = false
        };

        return wgpuDeviceCreateBuffer(m_app.wgpu_device, &descriptor);
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

    static constexpr WGPUColor bg_color = int_to_wgpu_color(0xFF101031);
    static constexpr std::array fill_colors
    {
        int_to_glm_color(0xFF420042),
        int_to_glm_color(0xFF631063),
        int_to_glm_color(0xFFFFEFEF),
    };

    static constexpr WGPUShaderModuleWGSLDescriptor mesh_shader_code_descriptor =
    {
        .chain =
        { 
            .next = nullptr,
            .sType = WGPUSType_ShaderModuleWGSLDescriptor
        },
        .code = R"WGSL(
struct vertex_transform
{
    projection: mat4x4f,
    morph_t: f32,
};

@group(0) @binding(0) var<uniform> transform: vertex_transform;
@group(0) @binding(1) var<uniform> color: vec4f;

@vertex
fn vs_main(@location(0) src_vertex: vec3f, @location(1) dst_vertex: vec3f)
    -> @builtin(position) vec4f
{
    let vertex_pos = mix(src_vertex, dst_vertex, transform.morph_t);
    return transform.projection * vec4f(vertex_pos, 1.0);
}

@fragment
fn fs_main() -> @location(0) vec4f
{
    return color;
}
        )WGSL"
    };

    static constexpr WGPUShaderModuleDescriptor shader_module_descriptor =
    {
        .nextInChain = &mesh_shader_code_descriptor.chain,
        .label = "ShaderModule"
    };

    static constexpr std::array binding_layout_entries
    {
        WGPUBindGroupLayoutEntry
        {
            .nextInChain = nullptr,
            .binding = 0,
            .visibility = WGPUShaderStage_Vertex,
            .buffer = 
            {
                .nextInChain = nullptr,
                .type = WGPUBufferBindingType_Uniform,
                .hasDynamicOffset = false,
                .minBindingSize = sizeof(glm::mat4) * 2
            },
            .sampler =
            {
                .nextInChain = nullptr,
                .type = WGPUSamplerBindingType_Undefined
            },
            .texture =
            {
                .nextInChain = nullptr,
                .sampleType = WGPUTextureSampleType_Undefined,
                .viewDimension = WGPUTextureViewDimension_Undefined,
                .multisampled = false
            },
            .storageTexture =
            {
                .nextInChain = nullptr,
                .access = WGPUStorageTextureAccess_Undefined,
                .format = WGPUTextureFormat_Undefined,
                .viewDimension = WGPUTextureViewDimension_Undefined
            }
        },
        WGPUBindGroupLayoutEntry
        {
            .nextInChain = nullptr,
            .binding = 1,
            .visibility = WGPUShaderStage_Fragment,
            .buffer = 
            {
                .nextInChain = nullptr,
                .type = WGPUBufferBindingType_Uniform,
                .hasDynamicOffset = true,
                .minBindingSize = sizeof(WGPUColor)
            },
            .sampler =
            {
                .nextInChain = nullptr,
                .type = WGPUSamplerBindingType_Undefined
            },
            .texture =
            {
                .nextInChain = nullptr,
                .sampleType = WGPUTextureSampleType_Undefined,
                .viewDimension = WGPUTextureViewDimension_Undefined,
                .multisampled = false
            },
            .storageTexture =
            {
                .nextInChain = nullptr,
                .access = WGPUStorageTextureAccess_Undefined,
                .format = WGPUTextureFormat_Undefined,
                .viewDimension = WGPUTextureViewDimension_Undefined
            }
        } 
    };

    static constexpr std::array cube_vertex_data
    {
        glm::vec3{2.0f, 2.0f, -2.0f},
        glm::vec3{2.0f, -2.0f, -2.0f},
        glm::vec3{-2.0f, -2.0f, -2.0f},
        glm::vec3{-2.0f, 2.0f, -2.0f},
        glm::vec3{2.0f, 2.0f, 2.0f},
        glm::vec3{2.0f, -2.0f, 2.0f},
        glm::vec3{-2.0f, -2.0f, 2.0f},
        glm::vec3{-2.0f, 2.0f, 2.0f},
        glm::vec3{2.0f, 0.0f, 0.0f},
        glm::vec3{0.0f, 2.0f, 0.0f},
        glm::vec3{0.0f, 0.0f, 2.0f},
        glm::vec3{-2.0f, 0.0f, 0.0f},
        glm::vec3{0.0f, -2.0f, 0.0f},
        glm::vec3{0.0f, 0.0f, -2.0f}
    };

    static constexpr std::array hedron_vertex_data
    {
        glm::vec3{2.0f, 2.0f, -2.0f},
        glm::vec3{2.0f, -2.0f, -2.0f},
        glm::vec3{-2.0f, -2.0f, -2.0f},
        glm::vec3{-2.0f, 2.0f, -2.0f},
        glm::vec3{2.0f, 2.0f, 2.0f},
        glm::vec3{2.0f, -2.0f, 2.0f},
        glm::vec3{-2.0f, -2.0f, 2.0f},
        glm::vec3{-2.0f, 2.0f, 2.0f},
        glm::vec3{3.5f, 0.0f, 0.0f},
        glm::vec3{0.0f, 3.5f, 0.0f},
        glm::vec3{0.0f, 0.0f, 3.5f},
        glm::vec3{-3.5f, 0.0f, 0.0f},
        glm::vec3{0.0f, -3.5f, 0.0f},
        glm::vec3{0.0f, 0.0f, -3.5f}
    };
    
    static constexpr std::array spikes_vertex_data
    {
        glm::vec3{1.0f, 1.0f, -1.0f},
        glm::vec3{1.0f, -1.0f, -1.0f},
        glm::vec3{-1.0f, -1.0f, -1.0f},
        glm::vec3{-1.0f, 1.0f, -1.0f},
        glm::vec3{1.0f, 1.0f, 1.0f},
        glm::vec3{1.0f, -1.0f, 1.0f},
        glm::vec3{-1.0f, -1.0f, 1.0f},
        glm::vec3{-1.0f, 1.0f, 1.0f},
        glm::vec3{1.0f, 0.0f, 0.0f},
        glm::vec3{0.0f, 1.0f, 0.0f},
        glm::vec3{0.0f, 0.0f, 4.5f},
        glm::vec3{-1.0f, 0.0f, 0.0f},
        glm::vec3{0.0f, -1.0f, 0.0f},
        glm::vec3{0.0f, 0.0f, -4.5f}
    };
    
    static constexpr std::array tile1_vertex_data
    {
        glm::vec3{2.0f, 2.0f, -0.5f},
        glm::vec3{2.0f, -2.0f, -0.5f},
        glm::vec3{-2.0f, -2.0f, -0.5f},
        glm::vec3{-2.0f, 2.0f, -0.5f},
        glm::vec3{2.0f, 2.0f, 0.5f},
        glm::vec3{2.0f, -2.0f, 0.5f},
        glm::vec3{-2.0f, -2.0f, 0.5f},
        glm::vec3{-2.0f, 2.0f, 0.5f},
        glm::vec3{2.0f, 0.0f, 0.0f},
        glm::vec3{0.0f, 2.0f, 0.0f},
        glm::vec3{0.0f, 0.0f, 0.5f},
        glm::vec3{-2.0f, 0.0f, 0.0f},
        glm::vec3{0.0f, -2.0f, 0.0f},
        glm::vec3{0.0f, 0.0f, -0.5f}
    };

    static constexpr std::array tile2_vertex_data
    {
        glm::vec3{2.0f, 2.0f, -0.5f},
        glm::vec3{2.0f, -2.0f, -0.5f},
        glm::vec3{-2.0f, -2.0f, -0.5f},
        glm::vec3{-2.0f, 2.0f, -0.5f},
        glm::vec3{2.0f, 2.0f, 0.5f},
        glm::vec3{2.0f, -2.0f, 0.5f},
        glm::vec3{-2.0f, -2.0f, 0.5f},
        glm::vec3{-2.0f, 2.0f, 0.5f},
        glm::vec3{2.8f, 0.0f, 0.0f},
        glm::vec3{0.0f, 2.8f, 0.0f},
        glm::vec3{0.0f, 0.0f, 0.5f},
        glm::vec3{-2.8f, 0.0f, 0.0f},
        glm::vec3{0.0f, -2.8f, 0.0f},
        glm::vec3{0.0f, 0.0f, -0.5f}
    };
    
    static constexpr auto vertex_data_size = cube_vertex_data.size();
    static_assert(hedron_vertex_data.size() == vertex_data_size);
    static_assert(spikes_vertex_data.size() == vertex_data_size);
    static_assert(tile1_vertex_data.size() == vertex_data_size);
    static_assert(tile2_vertex_data.size() == vertex_data_size);

    static constexpr std::array indices1_data
    {
        13u, 3u, 0u,
        13u, 1u, 2u,

        10u, 4u, 7u,
        10u, 6u, 5u,

        12u, 1u, 5u,
        12u, 6u, 2u,

        9u, 3u, 7u,
        9u, 4u, 0u,

        8u, 1u, 0u,
        8u, 4u, 5u,

        11u, 6u, 7u,
        11u, 3u, 2u
    };

    static constexpr std::array indices2_data
    {
      13, 0, 1,
      13, 2, 3,
      
      10, 7, 6,
      10, 5, 4,

      12, 5, 6,
      12, 2, 1,

      9, 7, 4,
      9, 0, 3,

      8, 0, 4,
      8, 5, 1,

      11, 7, 3,
      11, 2, 6
    };

    wgpu_app & m_app;
    WGPUShaderModule m_shader_module = nullptr;
    WGPURenderPipeline m_front_face_pipeline = nullptr;
    WGPURenderPipeline m_back_face_pipeline = nullptr;

    std::array<WGPUBuffer, 5> m_shape_vertex_buffers;

    WGPUBuffer m_indices1;
    WGPUBuffer m_indices2;

    WGPUBuffer m_transformation_uniform;
    WGPUBuffer m_color_uniform;
    WGPUBindGroup m_bind_group;

    glm::mat4 m_projection_matrix;
    float m_morph_time = 0.0f;
    std::size_t m_morph_index = 0;
}; /* class frame_renderer */

void print_wgpu_info(wgpu_app const & app)
{
    WGPUAdapterProperties properties
    {
        .nextInChain = nullptr
    };

    wgpuAdapterGetProperties(app.wgpu_adapter, &properties);
    
    std::cout << "Adapter properties:\n";
    std::cout << " - vendorID: " << properties.vendorID << '\n';
    std::cout << " - vendorName: " << properties.vendorName << '\n';
    std::cout << " - architecture: " << properties.architecture << '\n';
    std::cout << " - deviceId: " << properties.deviceID << '\n';
    std::cout << " - name: " << properties.name << '\n';
    std::cout << " - driverDescription: " << properties.driverDescription << '\n';
    std::cout << " - adapterType: " << properties.adapterType << '\n';
    std::cout << " - backendType: " << properties.backendType << '\n';
    std::cout << " - compatibilityMode: " << properties.compatibilityMode << '\n';

    WGPUSupportedLimits supported_limits;
    if(wgpuAdapterGetLimits(app.wgpu_adapter, &supported_limits))
    {
        std::cout << "Supported limits:\n" << supported_limits.limits;
    }
    else
    {
        std::cerr << "Can't retreive supported limits from adapter\n";
    }

    auto const feature_count = wgpuAdapterEnumerateFeatures(
        app.wgpu_adapter, nullptr);
    auto features = std::vector<WGPUFeatureName>{feature_count};
    wgpuAdapterEnumerateFeatures(app.wgpu_adapter, features.data());

    std::cout << "Features:\n";
    for(auto const feature: features)
    {
        std::cout << " - " << feature << '\n';
    }
}

int main(int argc, char const * argv[])
{
    try
    {
        wgpu_app app;

        print_wgpu_info(app);

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

            renderer.render(next_texture, elapsed_time, delta_time);

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
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", e.what(), nullptr);
        std::cerr << "Exception caught: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
