#include <Lyra/Utilities/Plugin.h>
#include <Lyra/Utilities/Logger.h>
#include <Lyra/Utilities/Sync.h>
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Graphics/RHITypes.h>

using namespace lyra;

// forward declarations for inlined plugins
FORWARD_DECLARE_API(lyra::mockgpu, RenderAPI)

using RenderPlugin        = Plugin<RenderAPI>;
using BuiltinRenderPlugin = BuiltinPlugin<RenderAPI>;

struct RHIBackendSlot
{
    Own<RenderPlugin>        plugin;
    Own<BuiltinRenderPlugin> builtin_plugin;
    RenderAPI*               api          = nullptr;
    Optional<RHIBackend>     backend_type = std::nullopt;
    GPUAdapter               adapter      = {};
    GPUDevice                device       = {};
    bool                     active       = false;
};

static Mutex& get_backends_mutex()
{
    static auto mtx = new Mutex();
    return *mtx;
}
static Array<RHIBackendSlot, 16> G_BACKENDS = {};
static Atomic<ubyte>             ACTIVE_BACKEND_ID{0};

#pragma region RHI
OwnedResource<RHI> RHI::init(const RHIDescriptor& descriptor)
{
    LockGuard<Mutex> lock(get_backends_mutex());

    int slot_id = -1;
    for (int i = 0; i < 16; ++i) {
        if (G_BACKENDS[i].active && G_BACKENDS[i].backend_type == descriptor.backend) {
            slot_id = i;
            break;
        }
    }

    if (slot_id == -1) {
        for (int i = 0; i < 16; ++i) {
            if (!G_BACKENDS[i].active) {
                slot_id = i;
                break;
            }
        }
    }

    if (slot_id == -1) {
        show_error("RHI", "Exceeded maximum number of active RHI backends (16)!");
        return {};
    }

    auto& slot        = G_BACKENDS[slot_id];
    slot.active       = true;
    slot.backend_type = descriptor.backend;

    switch (descriptor.backend) {
        case RHIBackend::D3D12:
            slot.plugin = make_own<RenderPlugin>("lyra-d3d12");
            slot.api    = slot.plugin->get_api();
            break;
        case RHIBackend::METAL:
            slot.plugin = make_own<RenderPlugin>("lyra-metal");
            slot.api    = slot.plugin->get_api();
            break;
        case RHIBackend::VULKAN:
            slot.plugin = make_own<RenderPlugin>("lyra-vulkan");
            slot.api    = slot.plugin->get_api();
            break;
        case RHIBackend::SOFTGPU:
            slot.plugin = make_own<RenderPlugin>("lyra-softgpu");
            slot.api    = slot.plugin->get_api();
            break;
        case RHIBackend::MOCKGPU:
            slot.builtin_plugin = make_own<BuiltinRenderPlugin>(
                lyra::mockgpu::create,
                lyra::mockgpu::prepare,
                lyra::mockgpu::cleanup);
            slot.api = slot.builtin_plugin->get_api();
            break;
        case RHIBackend::CUSTOM:
            slot.plugin = make_own<RenderPlugin>(descriptor.custom_backend);
            slot.api    = slot.plugin->get_api();
            break;
    }

    ACTIVE_BACKEND_ID.store(static_cast<ubyte>(slot_id), memory_order_release);

    OwnedResource<RHI> rhi(new RHI());
    rhi->id             = static_cast<ubyte>(slot_id);
    rhi->flags          = descriptor.flags;
    rhi->backend        = descriptor.backend;
    rhi->custom_backend = descriptor.custom_backend;
    slot.api->create_instance(descriptor);
    return rhi;
}

OwnedResource<RHI> RHI::init(StringView custom_backend, const RHIDescriptor& descriptor)
{
    RHIDescriptor desc  = descriptor;
    desc.backend        = RHIBackend::CUSTOM;
    desc.custom_backend = custom_backend;
    return init(desc);
}

RenderAPI* RHI::api()
{
    ubyte active_id = ACTIVE_BACKEND_ID.load(memory_order_relaxed);
    if (G_BACKENDS[active_id].active && G_BACKENDS[active_id].api) {
        return G_BACKENDS[active_id].api;
    }
    for (auto& slot : G_BACKENDS) {
        if (slot.active && slot.api) return slot.api;
    }
    return nullptr;
}

RenderAPI* RHI::api(uint rhi)
{
    if (rhi < G_BACKENDS.size() && G_BACKENDS[rhi].active) {
        return G_BACKENDS[rhi].api;
    }
    return RHI::api();
}

void RHI::destroy() const
{
    LockGuard<Mutex> lock(get_backends_mutex());

    if (id < G_BACKENDS.size()) {
        auto& slot = G_BACKENDS[id];
        if (slot.active && slot.api) {
            if (slot.device.valid()) {
                slot.api->wait_idle();
                slot.api->delete_device();
                slot.device.reset();
            }
            slot.api->delete_adapter();
            slot.api->delete_instance();
        }
        slot.builtin_plugin.reset();
        slot.plugin.reset();
        slot.api          = nullptr;
        slot.active       = false;
        slot.backend_type = std::nullopt;
        slot.adapter      = {};
        slot.device       = {};

        if (ACTIVE_BACKEND_ID.load(memory_order_relaxed) == id) {
            ubyte next_id = 0;
            for (usize i = 0; i < G_BACKENDS.size(); ++i) {
                if (G_BACKENDS[i].active && G_BACKENDS[i].api) {
                    next_id = static_cast<ubyte>(i);
                    break;
                }
            }
            ACTIVE_BACKEND_ID.store(next_id, memory_order_release);
        }
    }
}

void RHI::wait()
{
    if (auto api = RHI::api()) api->wait_idle();
}

void RHI::new_frame()
{
    if (auto api = RHI::api()) api->new_frame();
}

void RHI::end_frame()
{
    if (auto api = RHI::api()) api->end_frame();
}

GPUAdapter& RHI::get_current_adapter()
{
    ubyte active_id = ACTIVE_BACKEND_ID.load(memory_order_relaxed);
    if (G_BACKENDS[active_id].active && G_BACKENDS[active_id].adapter.valid()) {
        return G_BACKENDS[active_id].adapter;
    }
    for (auto& slot : G_BACKENDS) {
        if (slot.active && slot.adapter.valid()) return slot.adapter;
    }
    return G_BACKENDS[active_id].adapter;
}

GPUDevice& RHI::get_current_device()
{
    ubyte active_id = ACTIVE_BACKEND_ID.load(memory_order_relaxed);
    if (G_BACKENDS[active_id].active && G_BACKENDS[active_id].device.valid()) {
        return G_BACKENDS[active_id].device;
    }
    for (auto& slot : G_BACKENDS) {
        if (slot.active && slot.device.valid()) return slot.device;
    }
    return G_BACKENDS[active_id].device;
}

GPUAdapter RHI::request_adapter(const GPUAdapterDescriptor& descriptor) const
{
    GPUAdapterProps props;
    RHI::api(id)->create_adapter(props, descriptor);

    LockGuard<Mutex> lock(get_backends_mutex());
    auto&            adapter = G_BACKENDS[id].adapter;
    adapter.rhi              = id;
    adapter.info             = props.info;
    adapter.features         = props.features;
    adapter.limits           = props.limits;
    adapter.properties       = props.properties;
    return adapter;
}

GPUSurface RHI::request_surface(const GPUSurfaceDescriptor& descriptor) const
{
    GPUSurface surface;
    surface.rhi = id;
    RHI::api(id)->create_surface(surface.handle, descriptor);
    surface.handle = GPUSurfaceHandle::pack(surface.handle, id);
    return surface;
}
#pragma endregion RHI

#pragma region GPUAdapter
bool GPUAdapter::has_feature(GPUFeatureName feature) const
{
    switch (feature) {
        case GPUFeatureName::CORE_FEATURES_AND_LIMITS:
            return true;
        case GPUFeatureName::DEPTH_CLIP_CONTROL:
            return features.depth_clip_control;
        case GPUFeatureName::DEPTH32FLOAT_STENCIL8:
            return features.depth32float_stencil8;
        case GPUFeatureName::TEXTURE_COMPRESSION_BC:
            return features.texture_compression_bc;
        case GPUFeatureName::TEXTURE_COMPRESSION_BC_SLICED_3D:
            return features.texture_compression_bc_sliced_3d;
        case GPUFeatureName::TEXTURE_COMPRESSION_ETC2:
            return features.texture_compression_etc2;
        case GPUFeatureName::TEXTURE_COMPRESSION_ASTC:
            return features.texture_compression_astc;
        case GPUFeatureName::TEXTURE_COMPRESSION_ASTC_SLICED_3D:
            return features.texture_compression_astc_sliced_3d;
        case GPUFeatureName::TIMESTAMP_QUERY:
            return features.timestamp_query;
        case GPUFeatureName::INDIRECT_FIRST_INSTANCE:
            return features.indirect_first_instance;
        case GPUFeatureName::SHADER_F16:
            return features.shader_f16;
        case GPUFeatureName::RG11B10UFLOAT_PLUGINABLE:
            return features.rg11b10ufloat_renderable;
        case GPUFeatureName::BGRA8UNORM_STORAGE:
            return features.bgra8unorm_storage;
        case GPUFeatureName::FLOAT32_FILTERABLE:
            return features.float32_filterable;
        case GPUFeatureName::FLOAT32_BLENDABLE:
            return features.float32_blendable;
        case GPUFeatureName::CLIP_DISTANCES:
            return features.clip_distances;
        case GPUFeatureName::DUAL_SOURCE_BLENDING:
            return features.dual_source_blending;
        case GPUFeatureName::SUBGROUPS:
            return features.subgroups;
        case GPUFeatureName::TEXTURE_FORMATS_TIER1:
            return true;
        case GPUFeatureName::BINDLESS:
            return features.bindless;
        case GPUFeatureName::RAYTRACING:
            return features.raytracing;
        case GPUFeatureName::IMMEDIATES:
            return true;
        default:
            return false;
    }
}

GPUDevice GPUAdapter::request_device(const GPUDeviceDescriptor& descriptor)
{
    for (auto feature : descriptor.features) {
        if (!has_feature(feature)) {
            spdlog::error("Failed to create GPUDevice: Adapter lacks requested feature '{}'", to_string(feature));
            return {};
        }
    }

    LockGuard<Mutex> lock(get_backends_mutex());
    auto&            device = G_BACKENDS[rhi].device;
    device.rhi              = rhi;
    device.adapter_info     = info;
    device.features         = features;
    if (!RHI::api(rhi)->create_device(descriptor)) {
        device.reset();
        return {};
    }
    return device;
}
#pragma endregion GPUAdapter

#pragma region GPUSurface
GPUSurfaceTexture GPUSurface::get_current_texture() const
{
    GPUSurfaceTexture texture;
    texture.surface = handle;
    RHI::api(handle)->acquire_next_frame(handle.raw(), texture.texture, texture.view, texture.available, texture.complete, texture.suboptimal);
    texture.texture   = GPUTextureHandle::pack(texture.texture, handle.rhi());
    texture.view      = GPUTextureViewHandle::pack(texture.view, handle.rhi());
    texture.available = GPUFenceHandle::pack(texture.available, handle.rhi());
    texture.complete  = GPUFenceHandle::pack(texture.complete, handle.rhi());
    return texture;
}

GPUTextureFormat GPUSurface::get_current_format() const
{
    GPUTextureFormat format;
    RHI::api(handle)->get_surface_format(handle.raw(), format);
    return format;
}

GPUExtent2D GPUSurface::get_current_extent() const
{
    GPUExtent2D extent;
    RHI::api(handle)->get_surface_extent(handle.raw(), extent);
    return extent;
}

uint GPUSurface::get_image_count() const
{
    return RHI::api(handle)->get_surface_frames(handle.raw());
}

void GPUSurface::destroy() const
{
    if (valid()) {
        RHI::api(handle)->delete_surface(handle.raw());
    }
}
#pragma endregion GPUSurface

#pragma region GPUSurfaceTexture
void GPUSurfaceTexture::present() const
{
    RHI::api(surface)->present_curr_frame(surface.raw());
}
#pragma endregion GPUSurfaceTexture

#pragma region GPUDevice
GPUFence GPUDevice::create_fence() const
{
    GPUFence fence;
    RHI::api(rhi)->create_fence(fence.handle);
    fence.handle = GPUFenceHandle::pack(fence.handle, rhi);
    return fence;
}

GPUBuffer GPUDevice::create_buffer(const GPUBufferDescriptor& desc) const
{
    GPUBuffer buffer;
    RHI::api(rhi)->create_buffer(buffer.handle, desc);
    buffer.handle = GPUBufferHandle::pack(buffer.handle, rhi);
    buffer.size   = desc.size;
    buffer.usage  = desc.usage;
    return buffer;
}

GPUTexture GPUDevice::create_texture(const GPUTextureDescriptor& desc) const
{
    GPUTexture texture;
    RHI::api(rhi)->create_texture(texture.handle, desc);
    texture.handle          = GPUTextureHandle::pack(texture.handle, rhi);
    texture.width           = desc.size.width;
    texture.height          = desc.size.height;
    texture.depth           = desc.size.depth;
    texture.array_layers    = desc.array_layers;
    texture.mip_level_count = desc.mip_level_count;
    texture.sample_count    = desc.sample_count;
    texture.dimension       = desc.dimension;
    texture.format          = desc.format;
    texture.usage           = desc.usage;
    return texture;
}

GPUSampler GPUDevice::create_sampler(const GPUSamplerDescriptor& desc) const
{
    GPUSampler sampler;
    RHI::api(rhi)->create_sampler(sampler.handle, desc);
    sampler.handle = GPUSamplerHandle::pack(sampler.handle, rhi);
    return sampler;
}

GPUShaderModule GPUDevice::create_shader_module(const GPUShaderModuleDescriptor& desc) const
{
    GPUShaderModule module;
    RHI::api(rhi)->create_shader_module(module.handle, desc);
    module.handle = GPUShaderModuleHandle::pack(module.handle, rhi);
    return module;
}

GPUQuerySet GPUDevice::create_query_set(const GPUQuerySetDescriptor& desc) const
{
    GPUQuerySet query;
    RHI::api(rhi)->create_query_set(query.handle, desc);
    query.handle = GPUQuerySetHandle::pack(query.handle, rhi);
    return query;
}

GPUBlas GPUDevice::create_blas(const GPUBlasDescriptor& desc, const Vector<GPUBlasGeometrySizeDescriptor>& sizes) const
{
    GPUBlas blas;
    RHI::api(rhi)->create_blas(blas.handle, desc, sizes);
    blas.handle = GPUBlasHandle::pack(blas.handle, rhi);
    return blas;
}

GPUTlas GPUDevice::create_tlas(const GPUTlasDescriptor& desc) const
{
    GPUTlas tlas;
    RHI::api(rhi)->create_tlas(tlas.handle, desc);
    tlas.handle = GPUTlasHandle::pack(tlas.handle, rhi);
    return tlas;
}

GPUBindGroup GPUDevice::create_bind_group(const GPUBindGroupDescriptor& desc) const
{
    GPUBindGroup bind_group;
    RHI::api(rhi)->create_bind_group(bind_group.handle, desc);
    bind_group.handle = GPUBindGroupHandle::pack(bind_group.handle, rhi);
    return bind_group;
}

GPUBindGroupHeap GPUDevice::create_bind_group_heap(const GPUBindGroupHeapDescriptor& desc) const
{
    GPUBindGroupHeap heap;
    RHI::api(rhi)->create_bind_group_heap(heap.handle, desc);
    heap.handle = GPUBindGroupHeapHandle::pack(heap.handle, rhi);
    return heap;
}

GPUBindGroupLayout GPUDevice::create_bind_group_layout(const GPUBindGroupLayoutDescriptor& desc) const
{
    GPUBindGroupLayout layout;
    RHI::api(rhi)->create_bind_group_layout(layout.handle, desc);
    layout.handle = GPUBindGroupLayoutHandle::pack(layout.handle, rhi);
    return layout;
}

GPUPipelineLayout GPUDevice::create_pipeline_layout(const GPUPipelineLayoutDescriptor& desc) const
{
    for (auto& layout : desc.bind_group_layouts)
        assert(layout.valid() && "create_pipeline_layout() requires valid bind group layout!");

    GPUPipelineLayout layout;
    RHI::api(rhi)->create_pipeline_layout(layout.handle, desc);
    layout.handle = GPUPipelineLayoutHandle::pack(layout.handle, rhi);
    return layout;
}

GPURenderPipeline GPUDevice::create_render_pipeline(const GPURenderPipelineDescriptor& desc) const
{
    assert(desc.layout.valid() && "create_render_pipeline() requires valid pipeline layout!");
    assert(desc.vertex.module.valid() && "create_render_pipeline() requires valid vertex shader module!");
    assert(desc.fragment.module.valid() && "create_render_pipeline() requires valid fragment shader module!");

    GPURenderPipeline pipeline;
    RHI::api(rhi)->create_render_pipeline(pipeline.handle, desc);
    pipeline.handle = GPURenderPipelineHandle::pack(pipeline.handle, rhi);
    return pipeline;
}

GPUComputePipeline GPUDevice::create_compute_pipeline(const GPUComputePipelineDescriptor& desc) const
{
    assert(desc.layout.valid() && "create_compute_pipeline() requires valid pipeline layout!");
    assert(desc.compute.module.valid() && "create_compute_pipeline() requires valid compute shader module!");

    GPUComputePipeline pipeline;
    RHI::api(rhi)->create_compute_pipeline(pipeline.handle, desc);
    pipeline.handle = GPUComputePipelineHandle::pack(pipeline.handle, rhi);
    return pipeline;
}

GPURayTracingPipeline GPUDevice::create_raytracing_pipeline(const GPURayTracingPipelineDescriptor& desc) const
{
    assert(desc.layout.valid() && "create_raytracing_pipeline() requires valid pipeline layout!");

    GPURayTracingPipeline pipeline;
    RHI::api(rhi)->create_raytracing_pipeline(pipeline.handle, desc);
    pipeline.handle = GPURayTracingPipelineHandle::pack(pipeline.handle, rhi);
    return pipeline;
}

GPUCommandBuffer GPUDevice::create_command_buffer(const GPUCommandBufferDescriptor& desc) const
{
    GPUCommandBuffer command_buffer;
    RHI::api(rhi)->create_command_buffer(command_buffer.handle, desc);
    command_buffer.handle = GPUCommandEncoderHandle::pack(command_buffer.handle, rhi);
    return command_buffer;
}

GPUCommandBundle GPUDevice::create_command_bundle(const GPUCommandBundleDescriptor& desc) const
{
    GPUCommandBundle command_bundle;
    RHI::api(rhi)->create_command_bundle(command_bundle.handle, desc);
    command_bundle.handle = GPUCommandEncoderHandle::pack(command_bundle.handle, rhi);
    return command_bundle;
}

GPUBVHSizes GPUDevice::get_blas_sizes(GPUBlasHandle blas) const
{
    GPUBVHSizes sizes;
    RHI::api(rhi)->get_blas_sizes(blas.raw(), sizes);
    return sizes;
}

GPUBVHSizes GPUDevice::get_tlas_sizes(GPUTlasHandle tlas) const
{
    GPUBVHSizes sizes;
    RHI::api(rhi)->get_tlas_sizes(tlas.raw(), sizes);
    return sizes;
}

void GPUDevice::wait() const
{
    RHI::api(rhi)->wait_idle();
}

void GPUDevice::wait(GPUFence fence) const
{
    RHI::api(rhi)->wait_fence(fence.handle.raw());
}

void GPUDevice::destroy() const
{
    if (valid()) {
        RHI::api(rhi)->delete_device();
    }
}
#pragma endregion GPUSurface

#pragma region GPUBuffer
MappedBufferRange GPUBuffer::get_mapped_range() const
{
    MappedBufferRange range = {};
    RHI::api(handle)->get_mapped_range(handle.raw(), range);
    return range;
}

GPUMapState GPUBuffer::get_mapped_state() const
{
    GPUMapState state;
    RHI::api(handle)->get_mapped_state(handle.raw(), state);
    return state;
}

void GPUBuffer::map(GPUMapMode mode, GPUSize64 offset, GPUSize64 size)
{
    RHI::api(handle)->map_buffer(handle.raw(), mode, offset, size);
}

void GPUBuffer::unmap()
{
    RHI::api(handle)->unmap_buffer(handle.raw());
}

void GPUBuffer::destroy()
{
    if (valid()) {
        RHI::api(handle)->delete_buffer(handle.raw());
        handle.reset();
    }
}
#pragma endregion GPUBuffer

#pragma region GPUTexture
GPUTextureView GPUTexture::create_view()
{
    uint aspect   = 0;
    bool modified = false;
    if (is_depth_format(format)) {
        modified = true;
        aspect |= static_cast<uint>(GPUTextureAspect::DEPTH);
    }
    if (is_stencil_format(format)) {
        modified = true;
        aspect |= static_cast<uint>(GPUTextureAspect::STENCIL);
    }
    if (!modified) {
        aspect |= static_cast<uint>(GPUTextureAspect::COLOR);
    }

    GPUTextureViewDescriptor desc = {};
    desc.base_array_layer         = 0;
    desc.array_layer_count        = array_layers;
    desc.base_mip_level           = 0;
    desc.mip_level_count          = std::max(1u, mip_level_count);
    desc.aspect                   = static_cast<GPUTextureAspect>(aspect);
    desc.format                   = format;
    desc.usage                    = usage;
    switch (dimension) {
        case GPUTextureDimension::x1D:
            desc.dimension = GPUTextureViewDimension::x1D;
            break;
        case GPUTextureDimension::x2D:
            desc.dimension = GPUTextureViewDimension::x2D;
            break;
        case GPUTextureDimension::x3D:
            desc.dimension = GPUTextureViewDimension::x3D;
            break;
    }
    return create_view(desc);
}

GPUTextureView GPUTexture::create_view(GPUTextureViewDescriptor descriptor)
{
    GPUTextureView view;
    RHI::api(handle)->create_texture_view(view.handle, handle.raw(), descriptor);
    view.handle = GPUTextureViewHandle::pack(view.handle, handle.rhi());
    return view;
}

void GPUTexture::destroy()
{
    if (valid()) {
        RHI::api(handle)->delete_texture(handle.raw());
        handle.reset();
    }
}
#pragma endregion GPUTexture

#pragma region GPUTextureView
void GPUTextureView::destroy()
{
    if (valid()) {
        RHI::api(handle)->delete_texture_view(handle.raw());
        handle.reset();
    }
}
#pragma endregion GPUTextureView

#pragma region GPUSampler
void GPUSampler::destroy()
{
    if (valid()) {
        RHI::api(handle)->delete_sampler(handle.raw());
        handle.reset();
    }
}
#pragma endregion GPUSampler

#pragma region GPUFence
bool GPUFence::poll() const
{
    auto api = RHI::api(handle);
    return api && api->poll_fence ? api->poll_fence(handle.raw()) : true;
}

void GPUFence::wait() const
{
    if (auto api = RHI::api(handle)) api->wait_fence(handle.raw());
}

void GPUFence::destroy()
{
    if (valid()) {
        if (auto api = RHI::api(handle)) api->delete_fence(handle.raw());
        handle.reset();
    }
}
#pragma endregion GPUFence

#pragma region GPUShaderModule
void GPUShaderModule::destroy()
{
    if (valid()) {
        RHI::api(handle)->delete_shader_module(handle.raw());
        handle.reset();
    }
}
#pragma endregion GPUShaderModule

#pragma region GPUQuerySet
void GPUQuerySet::destroy()
{
    if (valid()) {
        RHI::api(handle)->delete_query_set(handle.raw());
        handle.reset();
    }
}
#pragma endregion GPUQuerySet

#pragma region GPUTlas
void GPUTlas::destroy()
{
    if (valid()) {
        RHI::api(handle)->delete_tlas(handle.raw());
        handle.reset();
    }
}
#pragma endregion GPUTlas

#pragma region GPUBlas
void GPUBlas::destroy()
{
    if (valid()) {
        RHI::api(handle)->delete_blas(handle.raw());
        handle.reset();
    }
}
#pragma endregion GPUBlas

#pragma region GPUBindGroupHeap
void GPUBindGroupHeap::reset()
{
    if (valid()) {
        RHI::api(handle)->reset_bind_group_heap(handle.raw());
    }
}

void GPUBindGroupHeap::destroy()
{
    if (valid()) {
        RHI::api(handle)->delete_bind_group_heap(handle.raw());
        handle.reset();
    }
}
#pragma endregion GPUBindGroupHeap

#pragma region GPUBindGroupLayout
void GPUBindGroupLayout::destroy()
{
    if (valid()) {
        RHI::api(handle)->delete_bind_group_layout(handle.raw());
        handle.reset();
    }
}
#pragma endregion GPUBindGroupLayout

#pragma region GPUPipelineLayout
void GPUPipelineLayout::destroy()
{
    if (valid()) {
        RHI::api(handle)->delete_pipeline_layout(handle.raw());
        handle.reset();
    }
}
#pragma endregion GPUPipelineLayout

#pragma region GPURenderPipeline
void GPURenderPipeline::destroy()
{
    if (valid()) {
        RHI::api(handle)->delete_render_pipeline(handle.raw());
        handle.reset();
    }
}
#pragma endregion GPURenderPipeline

#pragma region GPUComputePipeline
void GPUComputePipeline::destroy()
{
    if (valid()) {
        RHI::api(handle)->delete_compute_pipeline(handle.raw());
        handle.reset();
    }
}
#pragma endregion GPUComputePipeline

#pragma region GPURayTracingPipeline
void GPURayTracingPipeline::destroy()
{
    if (valid()) {
        RHI::api(handle)->delete_raytracing_pipeline(handle.raw());
        handle.reset();
    }
}
#pragma endregion GPURayTracingPipeline

#pragma region GPUCommandEncoder
void GPUCommandEncoder::insert_debug_marker(CString marker_label) const
{
    RHI::api(handle)->cmd_insert_debug_marker(handle.raw(), marker_label);
}

void GPUCommandEncoder::push_debug_group(CString group_label) const
{
    RHI::api(handle)->cmd_push_debug_group(handle.raw(), group_label);
}

void GPUCommandEncoder::pop_debug_group() const
{
    RHI::api(handle)->cmd_pop_debug_group(handle.raw());
}

void GPUCommandEncoder::wait(const GPUFence& fence, GPUBarrierSyncFlags sync) const
{
    RHI::api(handle)->cmd_wait_fence(handle.raw(), fence.handle.raw(), sync);
}

void GPUCommandEncoder::signal(const GPUFence& fence, GPUBarrierSyncFlags sync) const
{
    RHI::api(handle)->cmd_signal_fence(handle.raw(), fence.handle.raw(), sync);
}

void GPUCommandEncoder::set_pipeline(const GPURenderPipeline& pipeline) const
{
    RHI::api(handle)->cmd_set_render_pipeline(handle.raw(), pipeline.handle.raw());
}

void GPUCommandEncoder::set_pipeline(const GPUComputePipeline& pipeline) const
{
    RHI::api(handle)->cmd_set_compute_pipeline(handle.raw(), pipeline.handle.raw());
}

void GPUCommandEncoder::set_pipeline(const GPURayTracingPipeline& pipeline) const
{
    RHI::api(handle)->cmd_set_raytracing_pipeline(handle.raw(), pipeline.handle.raw());
}

void GPUCommandEncoder::set_bind_group(GPUIndex32 index, const GPUBindGroup& bind_group, const Vector<GPUBufferDynamicOffset>& dynamic_offsets) const
{
    RHI::api(handle)->cmd_set_bind_group(handle.raw(), index, bind_group.handle.raw(), dynamic_offsets);
}

void GPUCommandEncoder::set_immediates(uint offset, uint size, const void* data) const
{
    assert(offset % 4 == 0 && "Immediate offset must be a multiple of 4 bytes");
    assert(size % 4 == 0 && "Immediate size must be a multiple of 4 bytes");
    RHI::api(handle)->cmd_set_immediates(handle.raw(), offset, size, data);
}

void GPUCommandEncoder::dispatch_workgroups(GPUSize32 x, GPUSize32 y, GPUSize32 z) const
{
    RHI::api(handle)->cmd_dispatch_workgroups(handle.raw(), x, y, z);
}

void GPUCommandEncoder::dispatch_workgroups_indirect(const GPUBuffer& indirect_buffer, GPUSize64 indirect_offset) const
{
    RHI::api(handle)->cmd_dispatch_workgroups_indirect(handle.raw(), indirect_buffer.handle.raw(), indirect_offset);
}

void GPUCommandEncoder::set_index_buffer(const GPUBuffer& buffer, GPUIndexFormat index_format, GPUSize64 offset, GPUSize64 size) const
{
    RHI::api(handle)->cmd_set_index_buffer(handle.raw(), buffer.handle.raw(), index_format, offset, size);
}

void GPUCommandEncoder::set_vertex_buffer(GPUIndex32 slot, const GPUBuffer& buffer, GPUSize64 offset, GPUSize64 size) const
{
    RHI::api(handle)->cmd_set_vertex_buffer(handle.raw(), slot, buffer.handle.raw(), offset, size);
}

void GPUCommandEncoder::draw(GPUSize32 vertex_count, GPUSize32 instance_count, GPUSize32 first_vertex, GPUSize32 first_instance) const
{
    RHI::api(handle)->cmd_draw(handle.raw(), vertex_count, instance_count, first_vertex, first_instance);
}

void GPUCommandEncoder::draw_indexed(GPUSize32 index_count, GPUSize32 instance_count, GPUSize32 first_index, GPUSignedOffset32 base_vertex, GPUSize32 first_instance) const
{
    RHI::api(handle)->cmd_draw_indexed(handle.raw(), index_count, instance_count, first_index, base_vertex, first_instance);
}

void GPUCommandEncoder::draw_indirect(const GPUBuffer& indirect_buffer, GPUSize64 indirect_offset, GPUSize32 draw_count) const
{
    RHI::api(handle)->cmd_draw_indirect(handle.raw(), indirect_buffer.handle.raw(), indirect_offset, draw_count);
}

void GPUCommandEncoder::draw_indexed_indirect(const GPUBuffer& indirect_buffer, GPUSize64 indirect_offset, GPUSize32 draw_count) const
{
    RHI::api(handle)->cmd_draw_indexed_indirect(handle.raw(), indirect_buffer.handle.raw(), indirect_offset, draw_count);
}

void GPUCommandEncoder::begin_render_pass(const GPURenderPassDescriptor& descriptor) const
{
    RHI::api(handle)->cmd_begin_render_pass(handle.raw(), descriptor);
}

void GPUCommandEncoder::end_render_pass() const
{
    RHI::api(handle)->cmd_end_render_pass(handle.raw());
}

void GPUCommandEncoder::build_tlases(const GPUBuffer& scratch_buffer, GPUTlasBuildEntries entries) const
{
    RHI::api(handle)->cmd_build_tlases(handle.raw(), scratch_buffer.handle.raw(), entries);
}

void GPUCommandEncoder::build_blases(const GPUBuffer& scratch_buffer, GPUBlasBuildEntries entries) const
{
    RHI::api(handle)->cmd_build_blases(handle.raw(), scratch_buffer.handle.raw(), entries);
}

void GPUCommandEncoder::copy_buffer_to_buffer(const GPUBuffer& source, const GPUBuffer& destination, GPUSize64 size) const
{
    RHI::api(handle)->cmd_copy_buffer_to_buffer(handle.raw(), source.handle.raw(), 0u, destination.handle.raw(), 0u, size);
}

void GPUCommandEncoder::copy_buffer_to_buffer(const GPUBuffer& source, GPUSize64 source_offset, const GPUBuffer& destination, GPUSize64 destination_offset, GPUSize64 size) const
{
    RHI::api(handle)->cmd_copy_buffer_to_buffer(handle.raw(), source.handle.raw(), source_offset, destination.handle.raw(), destination_offset, size);
}

void GPUCommandEncoder::copy_buffer_to_texture(const GPUTexelCopyBufferInfo& source, const GPUTexelCopyTextureInfo& destination, const GPUExtent3D& copy_size) const
{
    RHI::api(handle)->cmd_copy_buffer_to_texture(handle.raw(), source, destination, copy_size);
}

void GPUCommandEncoder::copy_texture_to_buffer(const GPUTexelCopyTextureInfo& source, const GPUTexelCopyBufferInfo& destination, const GPUExtent3D& copy_size) const
{
    RHI::api(handle)->cmd_copy_texture_to_buffer(handle.raw(), source, destination, copy_size);
}

void GPUCommandEncoder::copy_texture_to_texture(const GPUTexelCopyTextureInfo& source, const GPUTexelCopyTextureInfo& destination, const GPUExtent3D& copy_size) const
{
    RHI::api(handle)->cmd_copy_texture_to_texture(handle.raw(), source, destination, copy_size);
}

void GPUCommandEncoder::clear_buffer(const GPUBuffer& buffer, GPUSize64 offset, GPUSize64 size) const
{
    RHI::api(handle)->cmd_clear_buffer(handle.raw(), buffer.handle.raw(), offset, size);
}

void GPUCommandEncoder::set_viewport(float x, float y, float w, float h, float min_depth, float max_depth) const
{
    RHI::api(handle)->cmd_set_viewport(handle.raw(), x, y, w, h, min_depth, max_depth);
}

void GPUCommandEncoder::set_scissor_rect(GPUIntegerCoordinate x, GPUIntegerCoordinate y, GPUIntegerCoordinate w, GPUIntegerCoordinate h) const
{
    RHI::api(handle)->cmd_set_scissor_rect(handle.raw(), x, y, w, h);
}

void GPUCommandEncoder::set_blend_constant(GPUColor color) const
{
    RHI::api(handle)->cmd_set_blend_constant(handle.raw(), color);
}

void GPUCommandEncoder::set_stencil_reference(GPUStencilValue reference) const
{
    RHI::api(handle)->cmd_set_stencil_reference(handle.raw(), reference);
}

void GPUCommandEncoder::begin_occlusion_query(GPUSize32 query_index) const
{
    RHI::api(handle)->cmd_begin_occlusion_query(handle.raw(), query_index);
}

void GPUCommandEncoder::end_occlusion_query() const
{
    RHI::api(handle)->cmd_end_occlusion_query(handle.raw());
}

void GPUCommandEncoder::write_timestamp(const GPUQuerySet& query_set, GPUSize32 query_index) const
{
    RHI::api(handle)->cmd_write_timestamp(handle.raw(), query_set.handle.raw(), query_index);
}

void GPUCommandEncoder::write_blas_properties(const GPUQuerySet& query_set, GPUSize32 query_index, const GPUBlas& blas) const
{
    RHI::api(handle)->cmd_write_blas_properties(handle.raw(), query_set.handle.raw(), query_index, blas.handle.raw());
}

void GPUCommandEncoder::resolve_query_set(GPUQuerySet query_set, GPUSize32 first_query, GPUSize32 query_count, const GPUBuffer& destination, GPUSize64 destination_offset) const
{
    RHI::api(handle)->cmd_resolve_query_set(handle.raw(), query_set.handle.raw(), first_query, query_count, destination.handle.raw(), destination_offset);
}

void GPUCommandEncoder::resource_barrier(GPUBufferBarrier barrier) const
{
    RHI::api(handle)->cmd_buffer_barrier(handle.raw(), barrier);
}

void GPUCommandEncoder::resource_barrier(GPUTextureBarrier barrier) const
{
    RHI::api(handle)->cmd_texture_barrier(handle.raw(), barrier);
}

void GPUCommandEncoder::resource_barrier(const Vector<GPUBufferBarrier>& barriers) const
{
    RHI::api(handle)->cmd_buffer_barrier(handle.raw(), barriers);
}

void GPUCommandEncoder::resource_barrier(const Vector<GPUTextureBarrier>& barriers) const
{
    RHI::api(handle)->cmd_texture_barrier(handle.raw(), barriers);
}
#pragma endregion GPUCommandEncoder

#pragma region GPUCommandBuffer
void GPUCommandBuffer::submit() const
{
    RHI::api(handle)->submit_command_buffer(handle.raw());
}
#pragma endregion GPUCommandBuffer
