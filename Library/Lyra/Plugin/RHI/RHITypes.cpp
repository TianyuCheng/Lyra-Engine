#include <Lyra/Common/Plugin.h>
#include <Lyra/Plugin/RHI/RHIAPI.h>
#include <Lyra/Plugin/RHI/RHITypes.h>

using namespace lyra;

using RenderPlugin = Plugin<RenderAPI>;

static Own<RenderPlugin> RENDER_PLUGIN;

#pragma region RHI
OwnedResource<RHI> RHI::init(const RHIDescriptor& descriptor)
{
    if (RENDER_PLUGIN.get()) {
        show_error("RHI", "Call RHI::init() exactly once!");
        exit(1);
    }

    switch (descriptor.backend) {
        case RHIBackend::D3D12:
            RENDER_PLUGIN = std::make_unique<RenderPlugin>("lyra-d3d12");
            break;
        case RHIBackend::METAL:
            RENDER_PLUGIN = std::make_unique<RenderPlugin>("lyra-metal");
            break;
        case RHIBackend::VULKAN:
            RENDER_PLUGIN = std::make_unique<RenderPlugin>("lyra-vulkan");
            break;
    }

    OwnedResource<RHI> rhi(new RHI());
    rhi->flags   = descriptor.flags;
    rhi->backend = descriptor.backend;
    rhi->window  = descriptor.window;
    RHI::api()->create_instance(descriptor);
    return rhi;
}

RenderAPI* RHI::api()
{
    return RENDER_PLUGIN->get_api();
}

void RHI::destroy() const
{
    RHI::api()->wait_idle();
    RHI::api()->delete_device();
    RHI::api()->delete_adapter();
    RHI::api()->delete_instance();
}

void RHI::wait()
{
    RHI::api()->wait_idle();
}

void RHI::new_frame()
{
    RHI::api()->new_frame();
}

void RHI::end_frame()
{
    RHI::api()->end_frame();
}

GPUAdapter& RHI::get_current_adapter()
{
    static GPUAdapter ADAPTER = {};
    return ADAPTER;
}

GPUDevice& RHI::get_current_device()
{
    static GPUDevice DEVICE = {};
    return DEVICE;
}

GPUAdapter RHI::request_adapter(const GPUAdapterDescriptor& descriptor) const
{
    GPUAdapterProps props;
    RHI::api()->create_adapter(props, descriptor);

    auto& adapter      = RHI::get_current_adapter();
    adapter.info       = props.info;
    adapter.features   = props.features;
    adapter.limits     = props.limits;
    adapter.properties = props.properties;
    return adapter;
}

GPUSurface RHI::request_surface(const GPUSurfaceDescriptor& descriptor) const
{
    GPUSurface surface;
    RHI::api()->create_surface(surface.handle, descriptor);
    return surface;
}
#pragma endregion RHI

#pragma region GPUAdapter
GPUDevice GPUAdapter::request_device(const GPUDeviceDescriptor& descriptor)
{
    auto& device        = RHI::get_current_device();
    device.adapter_info = info;
    device.features     = features;
    RHI::api()->create_device(descriptor);
    return device;
}
#pragma endregion GPUAdapter

#pragma region GPUSurface
GPUSurfaceTexture GPUSurface::get_current_texture() const
{
    GPUSurfaceTexture texture;
    texture.surface = handle;
    RHI::api()->acquire_next_frame(handle, texture.texture, texture.view, texture.available, texture.complete, texture.suboptimal);
    return texture;
}

GPUTextureFormat GPUSurface::get_current_format() const
{
    GPUTextureFormat format;
    RHI::api()->get_surface_format(handle, format);
    return format;
}

GPUExtent2D GPUSurface::get_current_extent() const
{
    GPUExtent2D extent;
    RHI::api()->get_surface_extent(handle, extent);
    return extent;
}

uint GPUSurface::get_image_count() const
{
    return RHI::api()->get_surface_frames(handle);
}

void GPUSurface::destroy() const
{
    RHI::api()->delete_surface(handle);
}
#pragma endregion GPUSurface

#pragma region GPUSurfaceTexture
void GPUSurfaceTexture::present() const
{
    RHI::api()->present_curr_frame(surface);
}
#pragma endregion GPUSurfaceTexture

#pragma region GPUDevice
GPUFence GPUDevice::create_fence() const
{
    GPUFence fence;
    RHI::api()->create_fence(fence.handle);
    return fence;
}

GPUBuffer GPUDevice::create_buffer(const GPUBufferDescriptor& desc) const
{
    GPUBuffer buffer;
    RHI::api()->create_buffer(buffer.handle, desc);
    buffer.size  = desc.size;
    buffer.usage = desc.usage;
    return buffer;
}

GPUTexture GPUDevice::create_texture(const GPUTextureDescriptor& desc) const
{
    GPUTexture texture;
    RHI::api()->create_texture(texture.handle, desc);
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
    RHI::api()->create_sampler(sampler.handle, desc);
    return sampler;
}

GPUShaderModule GPUDevice::create_shader_module(const GPUShaderModuleDescriptor& desc) const
{
    GPUShaderModule module;
    RHI::api()->create_shader_module(module.handle, desc);
    return module;
}

GPUQuerySet GPUDevice::create_query_set(const GPUQuerySetDescriptor& desc) const
{
    GPUQuerySet query;
    RHI::api()->create_query_set(query.handle, desc);
    return query;
}

GPUBlas GPUDevice::create_blas(const GPUBlasDescriptor& desc, const Vector<GPUBlasGeometrySizeDescriptor>& sizes) const
{
    GPUBlas blas;
    RHI::api()->create_blas(blas.handle, desc, sizes);
    return blas;
}

GPUTlas GPUDevice::create_tlas(const GPUTlasDescriptor& desc) const
{
    GPUTlas tlas;
    RHI::api()->create_tlas(tlas.handle, desc);
    return tlas;
}

GPUBindGroup GPUDevice::create_bind_group(const GPUBindGroupDescriptor& desc) const
{
    GPUBindGroup bind_group;
    RHI::api()->create_bind_group(bind_group.handle, desc);
    return bind_group;
}

GPUBindGroupHeap GPUDevice::create_bind_group_heap(const GPUBindGroupHeapDescriptor& desc) const
{
    GPUBindGroupHeap heap;
    RHI::api()->create_bind_group_heap(heap.handle, desc);
    return heap;
}

GPUBindGroupLayout GPUDevice::create_bind_group_layout(const GPUBindGroupLayoutDescriptor& desc) const
{
    GPUBindGroupLayout layout;
    RHI::api()->create_bind_group_layout(layout.handle, desc);
    return layout;
}

GPUPipelineLayout GPUDevice::create_pipeline_layout(const GPUPipelineLayoutDescriptor& desc) const
{
    for (auto& layout : desc.bind_group_layouts)
        assert(layout.valid() && "create_pipeline_layout() requires valid bind group layout!");

    GPUPipelineLayout layout;
    RHI::api()->create_pipeline_layout(layout.handle, desc);
    return layout;
}

GPURenderPipeline GPUDevice::create_render_pipeline(const GPURenderPipelineDescriptor& desc) const
{
    assert(desc.layout.valid() && "create_render_pipeline() requires valid pipeline layout!");
    assert(desc.vertex.module.valid() && "create_render_pipeline() requires valid vertex shader module!");
    assert(desc.fragment.module.valid() && "create_render_pipeline() requires valid fragment shader module!");

    GPURenderPipeline pipeline;
    RHI::api()->create_render_pipeline(pipeline.handle, desc);
    return pipeline;
}

GPUComputePipeline GPUDevice::create_compute_pipeline(const GPUComputePipelineDescriptor& desc) const
{
    assert(desc.layout.valid() && "create_compute_pipeline() requires valid pipeline layout!");
    assert(desc.compute.module.valid() && "create_compute_pipeline() requires valid compute shader module!");

    GPUComputePipeline pipeline;
    RHI::api()->create_compute_pipeline(pipeline.handle, desc);
    return pipeline;
}

GPURayTracingPipeline GPUDevice::create_raytracing_pipeline(const GPURayTracingPipelineDescriptor& desc) const
{
    assert(desc.layout.valid() && "create_raytracing_pipeline() requires valid pipeline layout!");

    GPURayTracingPipeline pipeline;
    RHI::api()->create_raytracing_pipeline(pipeline.handle, desc);
    return pipeline;
}

GPUCommandBuffer GPUDevice::create_command_buffer(const GPUCommandBufferDescriptor& desc) const
{
    GPUCommandBuffer command_buffer;
    RHI::api()->create_command_buffer(command_buffer.handle, desc);
    return command_buffer;
}

GPUCommandBundle GPUDevice::create_command_bundle(const GPUCommandBundleDescriptor& desc) const
{
    GPUCommandBundle command_bundle;
    RHI::api()->create_command_bundle(command_bundle.handle, desc);
    return command_bundle;
}

GPUBVHSizes GPUDevice::get_blas_sizes(GPUBlasHandle blas) const
{
    GPUBVHSizes sizes;
    RHI::api()->get_blas_sizes(blas, sizes);
    return sizes;
}

GPUBVHSizes GPUDevice::get_tlas_sizes(GPUTlasHandle tlas) const
{
    GPUBVHSizes sizes;
    RHI::api()->get_tlas_sizes(tlas, sizes);
    return sizes;
}

void GPUDevice::wait() const
{
    RHI::api()->wait_idle();
}

void GPUDevice::wait(GPUFence fence) const
{
    RHI::api()->wait_fence(fence.handle);
}

void GPUDevice::destroy() const
{
    RHI::api()->delete_device();
}
#pragma endregion GPUSurface

#pragma region GPUBuffer
MappedBufferRange GPUBuffer::get_mapped_range() const
{
    MappedBufferRange range = {};
    RHI::api()->get_mapped_range(handle, range);
    return range;
}

GPUMapState GPUBuffer::get_mapped_state() const
{
    GPUMapState state;
    RHI::api()->get_mapped_state(handle, state);
    return state;
}

void GPUBuffer::map(GPUMapMode mode, GPUSize64 offset, GPUSize64 size)
{
    RHI::api()->map_buffer(handle, mode, offset, size);
}

void GPUBuffer::unmap()
{
    RHI::api()->unmap_buffer(handle);
}

void GPUBuffer::destroy()
{
    RHI::api()->delete_buffer(handle);
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
    desc.mip_level_count          = std::min(mip_level_count, static_cast<GPUIntegerCoordinate>(std::log2(std::min(width, height))));
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
    RHI::api()->create_texture_view(view.handle, handle, descriptor);
    return view;
}

void GPUTexture::destroy()
{
    RHI::api()->delete_texture(handle);
    handle.reset();
}
#pragma endregion GPUTexture

#pragma region GPUTextureView
void GPUTextureView::destroy()
{
    RHI::api()->delete_texture_view(handle);
}
#pragma endregion GPUTextureView

#pragma region GPUSampler
void GPUSampler::destroy()
{
    RHI::api()->delete_sampler(handle);
    handle.reset();
}
#pragma endregion GPUSampler

#pragma region GPUFence
void GPUFence::wait() const
{
    RHI::api()->wait_fence(handle);
}

void GPUFence::destroy()
{
    RHI::api()->delete_fence(handle);
    handle.reset();
}
#pragma endregion GPUFence

#pragma region GPUShaderModule
void GPUShaderModule::destroy()
{
    RHI::api()->delete_shader_module(handle);
    handle.reset();
}
#pragma endregion GPUShaderModule

#pragma region GPUBindGroupHeap
void GPUBindGroupHeap::destroy()
{
    RHI::api()->delete_bind_group_heap(handle);
    handle.reset();
}
#pragma endregion GPUBindGroupHeap

#pragma region GPUBindGroupLayout
void GPUBindGroupLayout::destroy()
{
    RHI::api()->delete_bind_group_layout(handle);
    handle.reset();
}
#pragma endregion GPUBindGroupLayout

#pragma region GPUPipelineLayout
void GPUPipelineLayout::destroy()
{
    RHI::api()->delete_pipeline_layout(handle);
    handle.reset();
}
#pragma endregion GPUPipelineLayout

#pragma region GPURenderPipeline
void GPURenderPipeline::destroy()
{
    RHI::api()->delete_render_pipeline(handle);
    handle.reset();
}
#pragma endregion GPURenderPipeline

#pragma region GPUComputePipeline
void GPUComputePipeline::destroy()
{
    RHI::api()->delete_compute_pipeline(handle);
    handle.reset();
}
#pragma endregion GPUComputePipeline

#pragma region GPURayTracingPipeline
void GPURayTracingPipeline::destroy()
{
    RHI::api()->delete_raytracing_pipeline(handle);
    handle.reset();
}
#pragma endregion GPURayTracingPipeline

#pragma region GPUCommandEncoder
void GPUCommandEncoder::insert_debug_marker(CString marker_label) const
{
    RHI::api()->cmd_insert_debug_marker(handle, marker_label);
}

void GPUCommandEncoder::push_debug_group(CString group_label) const
{
    RHI::api()->cmd_push_debug_group(handle, group_label);
}

void GPUCommandEncoder::pop_debug_group() const
{
    RHI::api()->cmd_pop_debug_group(handle);
}

void GPUCommandEncoder::wait(const GPUFence& fence, GPUBarrierSyncFlags sync) const
{
    RHI::api()->cmd_wait_fence(handle, fence.handle, sync);
}

void GPUCommandEncoder::signal(const GPUFence& fence, GPUBarrierSyncFlags sync) const
{
    RHI::api()->cmd_signal_fence(handle, fence.handle, sync);
}

void GPUCommandEncoder::set_pipeline(const GPURenderPipeline& pipeline) const
{
    RHI::api()->cmd_set_render_pipeline(handle, pipeline.handle);
}

void GPUCommandEncoder::set_pipeline(const GPUComputePipeline& pipeline) const
{
    RHI::api()->cmd_set_compute_pipeline(handle, pipeline.handle);
}

void GPUCommandEncoder::set_pipeline(const GPURayTracingPipeline& pipeline) const
{
    RHI::api()->cmd_set_raytracing_pipeline(handle, pipeline.handle);
}

void GPUCommandEncoder::set_bind_group(GPUIndex32 index, const GPUBindGroup& bind_group, const Vector<GPUBufferDynamicOffset>& dynamic_offsets) const
{
    RHI::api()->cmd_set_bind_group(handle, index, bind_group, dynamic_offsets);
}

void GPUCommandEncoder::set_push_constants(GPUShaderStageFlags visibility, uint offset, uint size, void* data) const
{
    RHI::api()->cmd_set_push_constants(handle, visibility, offset, size, data);
}

void GPUCommandEncoder::dispatch_workgroups(GPUSize32 x, GPUSize32 y, GPUSize32 z) const
{
    RHI::api()->cmd_dispatch_workgroups(handle, x, y, z);
}

void GPUCommandEncoder::dispatch_workgroups_indirect(const GPUBuffer& indirect_buffer, GPUSize64 indirect_offset) const
{
    RHI::api()->cmd_dispatch_workgroups_indirect(handle, indirect_buffer, indirect_offset);
}

void GPUCommandEncoder::set_index_buffer(const GPUBuffer& buffer, GPUIndexFormat index_format, GPUSize64 offset, GPUSize64 size) const
{
    RHI::api()->cmd_set_index_buffer(handle, buffer.handle, index_format, offset, size);
}

void GPUCommandEncoder::set_vertex_buffer(GPUIndex32 slot, const GPUBuffer& buffer, GPUSize64 offset, GPUSize64 size) const
{
    RHI::api()->cmd_set_vertex_buffer(handle, slot, buffer.handle, offset, size);
}

void GPUCommandEncoder::draw(GPUSize32 vertex_count, GPUSize32 instance_count, GPUSize32 first_vertex, GPUSize32 first_instance) const
{
    RHI::api()->cmd_draw(handle, vertex_count, instance_count, first_vertex, first_instance);
}

void GPUCommandEncoder::draw_indexed(GPUSize32 index_count, GPUSize32 instance_count, GPUSize32 first_index, GPUSignedOffset32 base_vertex, GPUSize32 first_instance) const
{
    RHI::api()->cmd_draw_indexed(handle, index_count, instance_count, first_index, base_vertex, first_instance);
}

void GPUCommandEncoder::draw_indirect(const GPUBuffer& indirect_buffer, GPUSize64 indirect_offset, GPUSize32 draw_count) const
{
    RHI::api()->cmd_draw_indirect(handle, indirect_buffer.handle, indirect_offset, draw_count);
}

void GPUCommandEncoder::draw_indexed_indirect(const GPUBuffer& indirect_buffer, GPUSize64 indirect_offset, GPUSize32 draw_count) const
{
    RHI::api()->cmd_draw_indexed_indirect(handle, indirect_buffer.handle, indirect_offset, draw_count);
}

void GPUCommandEncoder::begin_render_pass(const GPURenderPassDescriptor& descriptor) const
{
    RHI::api()->cmd_begin_render_pass(handle, descriptor);
}

void GPUCommandEncoder::end_render_pass() const
{
    RHI::api()->cmd_end_render_pass(handle);
}

void GPUCommandEncoder::build_tlases(const GPUBuffer& scratch_buffer, GPUTlasBuildEntries entries) const
{
    RHI::api()->cmd_build_tlases(handle, scratch_buffer.handle, entries);
}

void GPUCommandEncoder::build_blases(const GPUBuffer& scratch_buffer, GPUBlasBuildEntries entries) const
{
    RHI::api()->cmd_build_blases(handle, scratch_buffer.handle, entries);
}

void GPUCommandEncoder::copy_buffer_to_buffer(const GPUBuffer& source, const GPUBuffer& destination, GPUSize64 size) const
{
    RHI::api()->cmd_copy_buffer_to_buffer(handle, source, 0u, destination, 0u, size);
}

void GPUCommandEncoder::copy_buffer_to_buffer(const GPUBuffer& source, GPUSize64 source_offset, const GPUBuffer& destination, GPUSize64 destination_offset, GPUSize64 size) const
{
    RHI::api()->cmd_copy_buffer_to_buffer(handle, source, source_offset, destination, destination_offset, size);
}

void GPUCommandEncoder::copy_buffer_to_texture(const GPUTexelCopyBufferInfo& source, const GPUTexelCopyTextureInfo& destination, const GPUExtent3D& copy_size) const
{
    RHI::api()->cmd_copy_buffer_to_texture(handle, source, destination, copy_size);
}

void GPUCommandEncoder::copy_texture_to_buffer(const GPUTexelCopyTextureInfo& source, const GPUTexelCopyBufferInfo& destination, const GPUExtent3D& copy_size) const
{
    RHI::api()->cmd_copy_texture_to_buffer(handle, source, destination, copy_size);
}

void GPUCommandEncoder::copy_texture_to_texture(const GPUTexelCopyTextureInfo& source, const GPUTexelCopyTextureInfo& destination, const GPUExtent3D& copy_size) const
{
    RHI::api()->cmd_copy_texture_to_texture(handle, source, destination, copy_size);
}

void GPUCommandEncoder::clear_buffer(const GPUBuffer& buffer, GPUSize64 offset, GPUSize64 size) const
{
    RHI::api()->cmd_clear_buffer(handle, buffer, offset, size);
}

void GPUCommandEncoder::set_viewport(float x, float y, float w, float h, float min_depth, float max_depth) const
{
    RHI::api()->cmd_set_viewport(handle, x, y, w, h, min_depth, max_depth);
}

void GPUCommandEncoder::set_scissor_rect(GPUIntegerCoordinate x, GPUIntegerCoordinate y, GPUIntegerCoordinate w, GPUIntegerCoordinate h) const
{
    RHI::api()->cmd_set_scissor_rect(handle, x, y, w, h);
}

void GPUCommandEncoder::set_blend_constant(GPUColor color) const
{
    RHI::api()->cmd_set_blend_constant(handle, color);
}

void GPUCommandEncoder::set_stencil_reference(GPUStencilValue reference) const
{
    RHI::api()->cmd_set_stencil_reference(handle, reference);
}

void GPUCommandEncoder::begin_occlusion_query(GPUSize32 query_index) const
{
    RHI::api()->cmd_begin_occlusion_query(handle, query_index);
}

void GPUCommandEncoder::end_occlusion_query() const
{
    RHI::api()->cmd_end_occlusion_query(handle);
}

void GPUCommandEncoder::write_timestamp(const GPUQuerySet& query_set, GPUSize32 query_index) const
{
    RHI::api()->cmd_write_timestamp(handle, query_set, query_index);
}

void GPUCommandEncoder::write_blas_properties(const GPUQuerySet& query_set, GPUSize32 query_index, const GPUBlas& blas) const
{
    RHI::api()->cmd_write_blas_properties(handle, query_set, query_index, blas);
}

void GPUCommandEncoder::resolve_query_set(GPUQuerySet query_set, GPUSize32 first_query, GPUSize32 query_count, const GPUBuffer& destination, GPUSize64 destination_offset) const
{
    RHI::api()->cmd_resolve_query_set(handle, query_set, first_query, query_count, destination, destination_offset);
}

void GPUCommandEncoder::resource_barrier(GPUBufferBarrier barrier) const
{
    RHI::api()->cmd_buffer_barrier(handle, barrier);
}

void GPUCommandEncoder::resource_barrier(GPUTextureBarrier barrier) const
{
    RHI::api()->cmd_texture_barrier(handle, barrier);
}

void GPUCommandEncoder::resource_barrier(const Vector<GPUBufferBarrier>& barriers) const
{
    RHI::api()->cmd_buffer_barrier(handle, barriers);
}

void GPUCommandEncoder::resource_barrier(const Vector<GPUTextureBarrier>& barriers) const
{
    RHI::api()->cmd_texture_barrier(handle, barriers);
}
#pragma endregion GPUCommandEncoder

#pragma region GPUCommandBuffer
void GPUCommandBuffer::submit() const
{
    RHI::api()->submit_command_buffer(handle);
}
#pragma endregion GPUCommandBuffer
