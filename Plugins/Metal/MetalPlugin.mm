#include "MetalUtils.h"
#include <Lyra/Common/String.h>
#include <Lyra/Common/Plugin.h>

using namespace lyra;

bool api::create_buffer(GPUBufferHandle& handle, const GPUBufferDescriptor& desc)
{
    auto rhi = get_rhi();
    auto obj = MetalBuffer(desc);
    auto ind = rhi->buffers.add(obj);
    handle   = GPUBufferHandle(ind);
    return obj.valid();
}

void api::delete_buffer(GPUBufferHandle handle)
{
    get_rhi()->buffers.remove(handle.value);
}

void api::map_buffer(GPUBufferHandle buffer, GPUMapMode, GPUSize64 offset, GPUSize64 size)
{
    auto& buf = fetch_resource(get_rhi()->buffers, buffer);
    buf.map(offset, size);
}

void api::unmap_buffer(GPUBufferHandle buffer)
{
    auto& buf = fetch_resource(get_rhi()->buffers, buffer);
    buf.unmap();
}

void api::get_mapped_state(GPUBufferHandle buffer, GPUMapState& state)
{
    auto& buf = fetch_resource(get_rhi()->buffers, buffer);
    state     = buf.mapped() ? GPUMapState::MAPPED : GPUMapState::UNMAPPED;
}

void api::get_mapped_range(GPUBufferHandle buffer, MappedBufferRange& range)
{
    auto& buf  = fetch_resource(get_rhi()->buffers, buffer);
    range.data = buf.mapped_data;
    range.size = buf.mapped_size;
}

bool api::create_sampler(GPUSamplerHandle& handle, const GPUSamplerDescriptor& desc)
{
    auto rhi = get_rhi();
    auto obj = MetalSampler(desc);
    auto ind = rhi->samplers.add(obj);
    handle   = GPUSamplerHandle(ind);
    return obj.valid();
}

void api::delete_sampler(GPUSamplerHandle handle)
{
    get_rhi()->samplers.remove(handle.value);
}

bool api::create_texture(GPUTextureHandle& handle, const GPUTextureDescriptor& desc)
{
    auto rhi = get_rhi();
    auto obj = MetalTexture(desc);
    auto ind = rhi->textures.add(obj);
    handle   = GPUTextureHandle(ind);
    return obj.valid();
}

void api::delete_texture(GPUTextureHandle handle)
{
    get_rhi()->textures.remove(handle.value);
}

bool api::create_texture_view(GPUTextureViewHandle& handle, GPUTextureHandle texture, const GPUTextureViewDescriptor& desc)
{
    auto  rhi = get_rhi();
    auto& tex = fetch_resource(rhi->textures, texture);
    auto  obj = MetalTextureView(tex, desc);
    auto  ind = rhi->views.add(obj);
    handle    = GPUTextureViewHandle(ind);
    return obj.valid();
}

void api::delete_texture_view(GPUTextureViewHandle handle)
{
    get_rhi()->views.remove(handle.value);
}

bool api::create_shader_module(GPUShaderModuleHandle& handle, const GPUShaderModuleDescriptor& desc)
{
    auto rhi = get_rhi();
    auto obj = MetalShader(desc);
    auto ind = rhi->shaders.add(obj);
    handle   = GPUShaderModuleHandle(ind);
    return obj.valid();
}

void api::delete_shader_module(GPUShaderModuleHandle handle)
{
    get_rhi()->shaders.remove(handle.value);
}

bool api::create_fence(GPUFenceHandle& handle)
{
    auto rhi = get_rhi();
    auto obj = MetalFence(false); // Create with signaled=false
    auto ind = rhi->fences.add(obj);
    handle   = GPUFenceHandle(ind);
    return obj.valid();
}

void api::delete_fence(GPUFenceHandle handle)
{
    get_rhi()->fences.remove(handle.value);
}

bool api::create_blas(GPUBlasHandle& handle, const GPUBlasDescriptor& desc, GPUBlasGeometrySizeDescriptors sizes)
{
    auto rhi = get_rhi();

    // check device support
    if (![rhi->device supportsRaytracing]) {
        get_logger()->error("Metal ray tracing is not supported on this device");
        return false;
    }

    auto obj = MetalBlas(desc, sizes);
    if (!obj.valid()) {
        return false;
    }

    auto ind = rhi->blases.add(obj);
    handle   = GPUBlasHandle(ind);
    return true;
}

void api::delete_blas(GPUBlasHandle handle)
{
    get_rhi()->blases.remove(handle.value);
}

bool api::create_tlas(GPUTlasHandle& handle, const GPUTlasDescriptor& desc)
{
    auto rhi = get_rhi();

    // check device support
    if (![rhi->device supportsRaytracing]) {
        get_logger()->error("Metal ray tracing is not supported on this device");
        return false;
    }

    auto obj = MetalTlas(desc);
    if (!obj.valid()) {
        return false;
    }

    auto ind = rhi->tlases.add(obj);
    handle   = GPUTlasHandle(ind);
    return true;
}

void api::delete_tlas(GPUTlasHandle handle)
{
    get_rhi()->tlases.remove(handle.value);
}

bool api::get_blas_sizes(GPUBlasHandle handle, GPUBVHSizes& sizes)
{
    auto  rhi  = get_rhi();
    auto& blas = fetch_resource(rhi->blases, handle);

    sizes.bvh_size    = static_cast<uint>(blas.sizes.accelerationStructureSize);
    sizes.build_size  = static_cast<uint>(blas.sizes.buildScratchBufferSize);
    sizes.update_size = static_cast<uint>(blas.sizes.refitScratchBufferSize);
    return true;
}

bool api::get_tlas_sizes(GPUTlasHandle handle, GPUBVHSizes& sizes)
{
    auto  rhi  = get_rhi();
    auto& tlas = fetch_resource(rhi->tlases, handle);

    sizes.bvh_size    = static_cast<uint>(tlas.sizes.accelerationStructureSize);
    sizes.build_size  = static_cast<uint>(tlas.sizes.buildScratchBufferSize);
    sizes.update_size = static_cast<uint>(tlas.sizes.refitScratchBufferSize);
    return true;
}

bool api::create_query_set(GPUQuerySetHandle& handle, const GPUQuerySetDescriptor& desc)
{
    auto rhi = get_rhi();
    auto obj = MetalQuerySet(desc);

    // for timestamp queries, check sample_buffer
    // for occlusion queries, check visibility_buffer
    bool is_valid = false;
    switch (desc.type) {
        case GPUQueryType::TIMESTAMP:
            is_valid = (obj.sample_buffer != nil);
            break;
        case GPUQueryType::OCCLUSION:
        case GPUQueryType::BLAS_PROPERTIES:
            is_valid = (obj.visibility_buffer != nil);
            break;
        case GPUQueryType::PIPELINE_STATISTICS:
            // Pipeline statistics have limited support, allow creation
            is_valid = true;
            break;
    }

    if (!is_valid)
        return false;

    auto ind = rhi->query_sets.add(obj);
    handle   = GPUQuerySetHandle(ind);
    return true;
}

void api::delete_query_set(GPUQuerySetHandle handle)
{
    get_rhi()->query_sets.remove(handle.value);
}

bool api::create_bind_group(GPUBindGroupHandle& handle, const GPUBindGroupDescriptor& desc)
{
    auto& heap = fetch_resource(get_rhi()->bind_group_heaps, desc.heap);
    handle     = heap.create_bind_group(desc);
    return handle.valid();
}

auto get_api_name() -> CString
{
    return "Metal";
}

// plugin entry points
LYRA_EXPORT auto prepare() -> void
{
    get_logger()->set_level(parse_log_level_from_env("LYRA_METAL_VERBOSITY"));

    // Metal framework initialization if needed
    // Most initialization happens in create_instance()
}

LYRA_EXPORT auto cleanup() -> void
{
    // cleanup global state if needed
}

LYRA_EXPORT auto create() -> RenderAPI
{
    auto api = RenderAPI{};

    api.get_api_name                     = get_api_name;
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
    api.map_buffer                       = api::map_buffer;
    api.unmap_buffer                     = api::unmap_buffer;
    api.get_mapped_state                 = api::get_mapped_state;
    api.get_mapped_range                 = api::get_mapped_range;
    api.create_command_buffer            = api::create_command_buffer;
    api.create_command_bundle            = api::create_command_bundle;
    api.submit_command_buffer            = api::submit_command_buffer;
    api.get_blas_sizes                   = api::get_blas_sizes;
    api.get_tlas_sizes                   = api::get_tlas_sizes;
    api.new_frame                        = api::new_frame;
    api.end_frame                        = api::end_frame;
    api.acquire_next_frame               = api::acquire_next_frame;
    api.present_curr_frame               = api::present_curr_frame;
    api.create_query_set                 = api::create_query_set;
    api.delete_query_set                 = api::delete_query_set;
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
    api.cmd_set_push_constants           = cmd::set_push_constants;
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
