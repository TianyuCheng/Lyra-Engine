#include <Lyra/Utilities/Plugin.h>
#include <Lyra/Utilities/Logger.h>
#include <Lyra/Compiler/SLCUtils.h>

#include "SoftCommon.h"
#include "SoftDevice.h"
#include "SoftCommandBuffer.h"

using namespace lyra;
using namespace lyra::softgpu;

CString api::get_api_name()
{
    return "SoftGPU";
}

bool api::create_instance(const RHIDescriptor& desc)
{
    get_logger()->trace("create_instance()");
    return get_device()->init(desc);
}

void api::delete_instance()
{
    get_logger()->trace("delete_instance()");
    get_device()->destroy();
}

bool api::create_adapter(GPUAdapterProps& adapter, const GPUAdapterDescriptor&)
{
    adapter = get_device()->adapter_props;
    return true;
}

void api::delete_adapter()
{
}

bool api::create_surface(GPUSurfaceHandle& surface, const GPUSurfaceDescriptor& desc)
{
    auto* dev  = get_device();
    auto  swap = SoftSwapchain(desc, dev->textures, dev->texture_views, dev->fences);
    auto  ind  = dev->swapchains.add(swap);
    surface    = GPUSurfaceHandle::create(ind);
    return true;
}

void api::delete_surface(GPUSurfaceHandle surface)
{
    get_device()->swapchains.remove(surface.to_slotmap_handle<SoftSwapchain>());
}

bool api::get_surface_extent(GPUSurfaceHandle surface, GPUExtent2D& extent)
{
    auto* dev  = get_device();
    auto* swap = dev->swapchains.find(surface.to_slotmap_handle<SoftSwapchain>());
    if (!swap) return false;
    extent = swap->extent;
    return true;
}

bool api::get_surface_format(GPUSurfaceHandle surface, GPUTextureFormat& format)
{
    auto* dev  = get_device();
    auto* swap = dev->swapchains.find(surface.to_slotmap_handle<SoftSwapchain>());
    if (!swap) return false;
    format = swap->format;
    return true;
}

uint api::get_surface_frames(GPUSurfaceHandle surface)
{
    auto* dev  = get_device();
    auto* swap = dev->swapchains.find(surface.to_slotmap_handle<SoftSwapchain>());
    if (!swap) return 0;
    return static_cast<uint>(swap->frames.size());
}

bool api::create_device(const GPUDeviceDescriptor&)
{
    return true;
}

void api::delete_device()
{
}

bool api::create_fence(GPUFenceHandle& fence)
{
    auto ind = get_device()->fences.add(SoftFence());
    fence    = GPUFenceHandle::create(ind);
    return true;
}

void api::delete_fence(GPUFenceHandle fence)
{
    get_device()->fences.remove(fence.to_slotmap_handle<SoftFence>());
}

bool api::create_buffer(GPUBufferHandle& buffer, const GPUBufferDescriptor& desc)
{
    auto ind = get_device()->buffers.add(SoftBuffer(desc));
    buffer   = GPUBufferHandle::create(ind);
    return true;
}

void api::delete_buffer(GPUBufferHandle buffer)
{
    get_device()->buffers.remove(buffer.to_slotmap_handle<SoftBuffer>());
}

bool api::create_sampler(GPUSamplerHandle& sampler, const GPUSamplerDescriptor& desc)
{
    auto ind = get_device()->samplers.add(SoftSampler(desc));
    sampler  = GPUSamplerHandle::create(ind);
    return true;
}

void api::delete_sampler(GPUSamplerHandle sampler)
{
    get_device()->samplers.remove(sampler.to_slotmap_handle<SoftSampler>());
}

bool api::create_texture(GPUTextureHandle& texture, const GPUTextureDescriptor& desc)
{
    auto ind = get_device()->textures.add(SoftTexture(desc));
    texture  = GPUTextureHandle::create(ind);
    return true;
}

void api::delete_texture(GPUTextureHandle texture)
{
    get_device()->textures.remove(texture.to_slotmap_handle<SoftTexture>());
}

bool api::create_texture_view(GPUTextureViewHandle& view, GPUTextureHandle texture, const GPUTextureViewDescriptor& desc)
{
    auto* dev = get_device();
    auto* tex = dev->textures.find(texture.to_slotmap_handle<SoftTexture>());
    if (!tex) return false;
    auto ind = dev->texture_views.add(SoftTextureView(texture, desc, *tex));
    view     = GPUTextureViewHandle::create(ind);
    return true;
}

void api::delete_texture_view(GPUTextureViewHandle view)
{
    get_device()->texture_views.remove(view.to_slotmap_handle<SoftTextureView>());
}

bool api::create_shader_module(GPUShaderModuleHandle& shader, const GPUShaderModuleDescriptor& desc)
{
    SoftShaderModule sm{};
    sm.label = desc.label;
    if (desc.data && desc.size > 0) {
        sm.bytecode.assign(desc.data, desc.data + desc.size);
        if (desc.size == sizeof(HostCallableShader)) {
            const auto* hcs = reinterpret_cast<const HostCallableShader*>(desc.data);
            if (hcs->magic == HOST_MAGIC) {
                sm.host_library = hcs->library;
                sm.host_func    = hcs->entry_func;
                sm.param_count  = hcs->param_count;
                std::memcpy(sm.params, hcs->params, sizeof(hcs->params));
            }
        }
    }
    auto ind = get_device()->shader_modules.add(sm);
    shader   = GPUShaderModuleHandle::create(ind);
    return true;
}

void api::delete_shader_module(GPUShaderModuleHandle shader)
{
    get_device()->shader_modules.remove(shader.to_slotmap_handle<SoftShaderModule>());
}

bool api::create_query_set(GPUQuerySetHandle& query, const GPUQuerySetDescriptor& desc)
{
    SoftQuerySet qs{};
    qs.type  = desc.type;
    qs.count = desc.count;
    qs.results.resize(desc.count, 0);
    qs.label = desc.label;
    auto ind = get_device()->query_sets.add(qs);
    query    = GPUQuerySetHandle::create(ind);
    return true;
}

void api::delete_query_set(GPUQuerySetHandle query)
{
    get_device()->query_sets.remove(query.to_slotmap_handle<SoftQuerySet>());
}

bool api::create_blas(GPUBlasHandle& blas, const GPUBlasDescriptor& desc, GPUBlasGeometrySizeDescriptors)
{
    auto ind = get_device()->blases.add(SoftBlas(desc));
    blas     = GPUBlasHandle::create(ind);
    return true;
}

void api::delete_blas(GPUBlasHandle blas)
{
    get_device()->blases.remove(blas.to_slotmap_handle<SoftBlas>());
}

bool api::create_tlas(GPUTlasHandle& tlas, const GPUTlasDescriptor& desc)
{
    auto ind = get_device()->tlases.add(SoftTlas(desc));
    tlas     = GPUTlasHandle::create(ind);
    return true;
}

void api::delete_tlas(GPUTlasHandle tlas)
{
    get_device()->tlases.remove(tlas.to_slotmap_handle<SoftTlas>());
}

bool api::create_pipeline_layout(GPUPipelineLayoutHandle& layout, const GPUPipelineLayoutDescriptor& desc)
{
    auto ind = get_device()->pipeline_layouts.add(SoftPipelineLayout(desc));
    layout   = GPUPipelineLayoutHandle::create(ind);
    return true;
}

void api::delete_pipeline_layout(GPUPipelineLayoutHandle layout)
{
    get_device()->pipeline_layouts.remove(layout.to_slotmap_handle<SoftPipelineLayout>());
}

bool api::create_render_pipeline(GPURenderPipelineHandle& pipeline, const GPURenderPipelineDescriptor& desc)
{
    auto ind = get_device()->render_pipelines.add(SoftRenderPipeline(desc));
    pipeline = GPURenderPipelineHandle::create(ind);
    return true;
}

void api::delete_render_pipeline(GPURenderPipelineHandle pipeline)
{
    get_device()->render_pipelines.remove(pipeline.to_slotmap_handle<SoftRenderPipeline>());
}

bool api::create_compute_pipeline(GPUComputePipelineHandle& pipeline, const GPUComputePipelineDescriptor& desc)
{
    auto ind = get_device()->compute_pipelines.add(SoftComputePipeline(desc));
    pipeline = GPUComputePipelineHandle::create(ind);
    return true;
}

void api::delete_compute_pipeline(GPUComputePipelineHandle pipeline)
{
    get_device()->compute_pipelines.remove(pipeline.to_slotmap_handle<SoftComputePipeline>());
}

bool api::create_raytracing_pipeline(GPURayTracingPipelineHandle& pipeline, const GPURayTracingPipelineDescriptor& desc)
{
    auto ind = get_device()->raytracing_pipelines.add(SoftRayTracingPipeline(desc));
    pipeline = GPURayTracingPipelineHandle::create(ind);
    return true;
}

void api::delete_raytracing_pipeline(GPURayTracingPipelineHandle pipeline)
{
    get_device()->raytracing_pipelines.remove(pipeline.to_slotmap_handle<SoftRayTracingPipeline>());
}

bool api::create_bind_group(GPUBindGroupHandle& bg, const GPUBindGroupDescriptor& desc)
{
    auto ind = get_device()->bind_groups.add(SoftBindGroup(desc));
    bg       = GPUBindGroupHandle::create(ind);
    return true;
}

bool api::create_bind_group_layout(GPUBindGroupLayoutHandle& layout, const GPUBindGroupLayoutDescriptor& desc)
{
    auto ind = get_device()->bind_group_layouts.add(SoftBindGroupLayout(desc));
    layout   = GPUBindGroupLayoutHandle::create(ind);
    return true;
}

void api::delete_bind_group_layout(GPUBindGroupLayoutHandle layout)
{
    get_device()->bind_group_layouts.remove(layout.to_slotmap_handle<SoftBindGroupLayout>());
}

bool api::create_bind_group_heap(GPUBindGroupHeapHandle& heap, const GPUBindGroupHeapDescriptor&)
{
    heap = GPUBindGroupHeapHandle(get_device()->heap_counter.fetch_add(1));
    return true;
}

void api::delete_bind_group_heap(GPUBindGroupHeapHandle)
{
}

void api::reset_bind_group_heap(GPUBindGroupHeapHandle)
{
}

void api::new_frame()
{
}

void api::end_frame()
{
}

bool api::acquire_next_frame(GPUSurfaceHandle surface, GPUTextureHandle& texture, GPUTextureViewHandle& view,
    GPUFenceHandle& available, GPUFenceHandle& complete, bool& suboptimal)
{
    auto* dev  = get_device();
    auto* swap = dev->swapchains.find(surface.to_slotmap_handle<SoftSwapchain>());
    if (!swap) return false;
    return swap->acquire_next_frame(texture, view, available, complete, suboptimal);
}

bool api::present_curr_frame(GPUSurfaceHandle surface)
{
    auto* dev  = get_device();
    auto* swap = dev->swapchains.find(surface.to_slotmap_handle<SoftSwapchain>());
    if (!swap) return false;
    return swap->present(dev->textures);
}

void api::get_mapped_state(GPUBufferHandle buffer, GPUMapState& state)
{
    auto* b = get_device()->buffers.find(buffer.to_slotmap_handle<SoftBuffer>());
    if (b)
        state = b->map_state;
    else
        state = GPUMapState::UNMAPPED;
}

void api::get_mapped_range(GPUBufferHandle buffer, MappedBufferRange& range)
{
    auto* b = get_device()->buffers.find(buffer.to_slotmap_handle<SoftBuffer>());
    if (b)
        range = b->get_mapped_range();
    else
        range = MappedBufferRange{};
}

void api::map_buffer(GPUBufferHandle buffer, GPUMapMode mode, GPUSize64 offset, GPUSize64 size)
{
    auto* b = get_device()->buffers.find(buffer.to_slotmap_handle<SoftBuffer>());
    if (b) b->map(mode, offset, size);
}

void api::unmap_buffer(GPUBufferHandle buffer)
{
    auto* b = get_device()->buffers.find(buffer.to_slotmap_handle<SoftBuffer>());
    if (b) b->unmap();
}

void api::wait_idle()
{
}

void api::wait_fence(GPUFenceHandle fence)
{
    auto* f = get_device()->fences.find(fence.to_slotmap_handle<SoftFence>());
    if (f) f->wait();
}

bool api::poll_fence(GPUFenceHandle fence)
{
    auto* f = get_device()->fences.find(fence.to_slotmap_handle<SoftFence>());
    if (f) return f->poll();
    return true;
}

bool api::get_blas_sizes(GPUBlasHandle, GPUBVHSizes& sizes)
{
    sizes.bvh_size    = 1024 * 1024;
    sizes.build_size  = 1024 * 1024;
    sizes.update_size = 1024 * 1024;
    return true;
}

bool api::get_tlas_sizes(GPUTlasHandle, GPUBVHSizes& sizes)
{
    sizes.bvh_size    = 1024 * 1024;
    sizes.build_size  = 1024 * 1024;
    sizes.update_size = 1024 * 1024;
    return true;
}

bool api::create_command_buffer(GPUCommandEncoderHandle& cmdbuffer, const GPUCommandBufferDescriptor&)
{
    auto* dev = get_device();
    ulong id  = dev->cmd_counter.fetch_add(1);
    cmdbuffer = GPUCommandEncoderHandle(id);
    dev->command(cmdbuffer).reset();
    return true;
}

bool api::create_command_bundle(GPUCommandEncoderHandle& cmdbuffer, const GPUCommandBundleDescriptor&)
{
    auto* dev = get_device();
    ulong id  = dev->cmd_counter.fetch_add(1);
    cmdbuffer = GPUCommandEncoderHandle(id);
    dev->command(cmdbuffer).reset();
    return true;
}

bool api::submit_command_buffer(GPUCommandEncoderHandle cmdbuffer)
{
    auto* dev = get_device();
    auto  it  = dev->command_buffers.find(cmdbuffer.value);
    if (it != dev->command_buffers.end()) {
        it->second->execute_commands(*dev);
        it->second->reset();
    }
    return true;
}

LYRA_EXPORT void prepare()
{
    // prepare software backend
}

LYRA_EXPORT void cleanup()
{
    // cleanup software backend
}

LYRA_EXPORT RenderAPI create()
{
    auto api                             = RenderAPI{};
    api.get_api_name                     = api::get_api_name;
    api.create_instance                  = api::create_instance;
    api.delete_instance                  = api::delete_instance;
    api.create_adapter                   = api::create_adapter;
    api.delete_adapter                   = api::delete_adapter;
    api.create_device                    = api::create_device;
    api.delete_device                    = api::delete_device;
    api.create_surface                   = api::create_surface;
    api.delete_surface                   = api::delete_surface;
    api.get_surface_extent               = api::get_surface_extent;
    api.get_surface_format               = api::get_surface_format;
    api.get_surface_frames               = api::get_surface_frames;
    api.create_buffer                    = api::create_buffer;
    api.delete_buffer                    = api::delete_buffer;
    api.create_texture                   = api::create_texture;
    api.delete_texture                   = api::delete_texture;
    api.create_texture_view              = api::create_texture_view;
    api.delete_texture_view              = api::delete_texture_view;
    api.create_sampler                   = api::create_sampler;
    api.delete_sampler                   = api::delete_sampler;
    api.create_fence                     = api::create_fence;
    api.delete_fence                     = api::delete_fence;
    api.create_shader_module             = api::create_shader_module;
    api.delete_shader_module             = api::delete_shader_module;
    api.create_blas                      = api::create_blas;
    api.delete_blas                      = api::delete_blas;
    api.create_tlas                      = api::create_tlas;
    api.delete_tlas                      = api::delete_tlas;
    api.create_pipeline_layout           = api::create_pipeline_layout;
    api.delete_pipeline_layout           = api::delete_pipeline_layout;
    api.create_render_pipeline           = api::create_render_pipeline;
    api.delete_render_pipeline           = api::delete_render_pipeline;
    api.create_compute_pipeline          = api::create_compute_pipeline;
    api.delete_compute_pipeline          = api::delete_compute_pipeline;
    api.create_raytracing_pipeline       = api::create_raytracing_pipeline;
    api.delete_raytracing_pipeline       = api::delete_raytracing_pipeline;
    api.create_bind_group                = api::create_bind_group;
    api.create_bind_group_layout         = api::create_bind_group_layout;
    api.delete_bind_group_layout         = api::delete_bind_group_layout;
    api.create_bind_group_heap           = api::create_bind_group_heap;
    api.delete_bind_group_heap           = api::delete_bind_group_heap;
    api.reset_bind_group_heap            = api::reset_bind_group_heap;
    api.wait_idle                        = api::wait_idle;
    api.wait_fence                       = api::wait_fence;
    api.poll_fence                       = api::poll_fence;
    api.new_frame                        = api::new_frame;
    api.end_frame                        = api::end_frame;
    api.map_buffer                       = api::map_buffer;
    api.unmap_buffer                     = api::unmap_buffer;
    api.get_mapped_state                 = api::get_mapped_state;
    api.get_mapped_range                 = api::get_mapped_range;
    api.create_command_buffer            = api::create_command_buffer;
    api.create_command_bundle            = api::create_command_bundle;
    api.submit_command_buffer            = api::submit_command_buffer;
    api.get_blas_sizes                   = api::get_blas_sizes;
    api.get_tlas_sizes                   = api::get_tlas_sizes;
    api.acquire_next_frame               = api::acquire_next_frame;
    api.present_curr_frame               = api::present_curr_frame;
    api.cmd_insert_debug_marker          = cmd::insert_debug_marker;
    api.cmd_push_debug_group             = cmd::push_debug_group;
    api.cmd_pop_debug_group              = cmd::pop_debug_group;
    api.cmd_wait_fence                   = cmd::wait_fence;
    api.cmd_signal_fence                 = cmd::signal_fence;
    api.cmd_begin_render_pass            = cmd::begin_render_pass;
    api.cmd_end_render_pass              = cmd::end_render_pass;
    api.cmd_set_render_pipeline          = cmd::set_render_pipeline;
    api.cmd_set_compute_pipeline         = cmd::set_compute_pipeline;
    api.cmd_set_raytracing_pipeline      = cmd::set_raytracing_pipeline;
    api.cmd_set_bind_group               = cmd::set_bind_group;
    api.cmd_set_immediates               = cmd::set_immediates;
    api.cmd_set_index_buffer             = cmd::set_index_buffer;
    api.cmd_set_vertex_buffer            = cmd::set_vertex_buffer;
    api.cmd_draw                         = cmd::draw;
    api.cmd_draw_indexed                 = cmd::draw_indexed;
    api.cmd_draw_indirect                = cmd::draw_indirect;
    api.cmd_draw_indexed_indirect        = cmd::draw_indexed_indirect;
    api.cmd_dispatch_workgroups          = cmd::dispatch_workgroups;
    api.cmd_dispatch_workgroups_indirect = cmd::dispatch_workgroups_indirect;
    api.cmd_copy_buffer_to_buffer        = cmd::copy_buffer_to_buffer;
    api.cmd_copy_buffer_to_texture       = cmd::copy_buffer_to_texture;
    api.cmd_copy_texture_to_buffer       = cmd::copy_texture_to_buffer;
    api.cmd_copy_texture_to_texture      = cmd::copy_texture_to_texture;
    api.cmd_clear_buffer                 = cmd::clear_buffer;
    api.cmd_clear_texture                = cmd::clear_texture;
    api.cmd_set_viewport                 = cmd::set_viewport;
    api.cmd_set_scissor_rect             = cmd::set_scissor_rect;
    api.cmd_set_blend_constant           = cmd::set_blend_constant;
    api.cmd_set_stencil_reference        = cmd::set_stencil_reference;
    api.cmd_begin_occlusion_query        = cmd::begin_occlusion_query;
    api.cmd_end_occlusion_query          = cmd::end_occlusion_query;
    api.cmd_write_timestamp              = cmd::write_timestamp;
    api.cmd_write_blas_properties        = cmd::write_blas_properties;
    api.cmd_resolve_query_set            = cmd::resolve_query_set;
    api.cmd_memory_barrier               = cmd::memory_barrier;
    api.cmd_buffer_barrier               = cmd::buffer_barrier;
    api.cmd_texture_barrier              = cmd::texture_barrier;
    api.cmd_build_tlases                 = cmd::build_tlases;
    api.cmd_build_blases                 = cmd::build_blases;
    api.cmd_copy_blas                    = cmd::copy_blas;
    return api;
}
