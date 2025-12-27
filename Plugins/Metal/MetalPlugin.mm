#include "MetalUtils.h"
#include <Lyra/Common/String.h>
#include <Lyra/Common/Plugin.h>

using namespace lyra;

auto get_api_name() -> CString
{
    return "Metal";
}

// Plugin entry points
LYRA_EXPORT auto prepare() -> void
{
    get_logger()->set_level(parse_log_level_from_env("LYRA_METAL_VERBOSITY"));

    // Metal framework initialization if needed
    // Most initialization happens in create_instance()
}

LYRA_EXPORT auto cleanup() -> void
{
    // Cleanup global state if needed
}

LYRA_EXPORT auto create() -> RenderAPI
{
    auto api = RenderAPI{};

    // API name
    api.get_api_name = get_api_name;

    // Instance management
    api.create_instance = api::create_instance;
    api.delete_instance = api::delete_instance;

    // Adapter management
    api.create_adapter = api::create_adapter;
    api.delete_adapter = api::delete_adapter;

    // Device management
    api.create_device = api::create_device;
    api.delete_device = api::delete_device;

    // Surface management
    api.create_surface = api::create_surface;
    api.delete_surface = api::delete_surface;
    api.get_surface_extent = api::get_surface_extent;
    api.get_surface_format = api::get_surface_format;
    api.get_surface_frames = api::get_surface_frames;

    // Buffer management
    api.create_buffer = api::create_buffer;
    api.delete_buffer = api::delete_buffer;

    // Texture management
    api.create_texture = api::create_texture;
    api.delete_texture = api::delete_texture;
    api.create_texture_view = api::create_texture_view;
    api.delete_texture_view = api::delete_texture_view;

    // Sampler management
    api.create_sampler = api::create_sampler;
    api.delete_sampler = api::delete_sampler;

    // Fence management
    api.create_fence = api::create_fence;
    api.delete_fence = api::delete_fence;

    // Shader module management
    api.create_shader_module = api::create_shader_module;
    api.delete_shader_module = api::delete_shader_module;

    // Acceleration structure management
    api.create_blas = api::create_blas;
    api.delete_blas = api::delete_blas;
    api.create_tlas = api::create_tlas;
    api.delete_tlas = api::delete_tlas;

    // Pipeline layout management
    api.create_pipeline_layout = api::create_pipeline_layout;
    api.delete_pipeline_layout = api::delete_pipeline_layout;

    // Pipeline management
    api.create_render_pipeline = api::create_render_pipeline;
    api.delete_render_pipeline = api::delete_render_pipeline;
    api.create_compute_pipeline = api::create_compute_pipeline;
    api.delete_compute_pipeline = api::delete_compute_pipeline;
    api.create_raytracing_pipeline = api::create_raytracing_pipeline;
    api.delete_raytracing_pipeline = api::delete_raytracing_pipeline;

    // Bind group management
    api.create_bind_group = api::create_bind_group;
    api.create_bind_group_layout = api::create_bind_group_layout;
    api.delete_bind_group_layout = api::delete_bind_group_layout;
    api.create_bind_group_heap = api::create_bind_group_heap;
    api.delete_bind_group_heap = api::delete_bind_group_heap;
    api.reset_bind_group_heap = api::reset_bind_group_heap;

    // Synchronization
    api.wait_idle = api::wait_idle;
    api.wait_fence = api::wait_fence;

    // Buffer mapping
    api.map_buffer = api::map_buffer;
    api.unmap_buffer = api::unmap_buffer;
    api.get_mapped_state = api::get_mapped_state;
    api.get_mapped_range = api::get_mapped_range;

    // Command buffer management
    api.create_command_buffer = api::create_command_buffer;
    api.create_command_bundle = api::create_command_bundle;
    api.submit_command_buffer = api::submit_command_buffer;

    // Acceleration structure queries
    api.get_blas_sizes = api::get_blas_sizes;
    api.get_tlas_sizes = api::get_tlas_sizes;

    // Frame management
    api.new_frame = api::new_frame;
    api.end_frame = api::end_frame;
    api.acquire_next_frame = api::acquire_next_frame;
    api.present_curr_frame = api::present_curr_frame;

    // Query set management
    api.create_query_set = api::create_query_set;
    api.delete_query_set = api::delete_query_set;

    // Command recording functions
    api.cmd_insert_debug_marker = cmd::insert_debug_marker;
    api.cmd_push_debug_group = cmd::push_debug_group;
    api.cmd_pop_debug_group = cmd::pop_debug_group;
    api.cmd_wait_fence = cmd::wait_fence;
    api.cmd_signal_fence = cmd::signal_fence;
    api.cmd_begin_render_pass = cmd::begin_render_pass;
    api.cmd_end_render_pass = cmd::end_render_pass;
    api.cmd_set_render_pipeline = cmd::set_render_pipeline;
    api.cmd_set_compute_pipeline = cmd::set_compute_pipeline;
    api.cmd_set_raytracing_pipeline = cmd::set_raytracing_pipeline;
    api.cmd_set_bind_group = cmd::set_bind_group;
    api.cmd_set_push_constants = cmd::set_push_constants;
    api.cmd_set_index_buffer = cmd::set_index_buffer;
    api.cmd_set_vertex_buffer = cmd::set_vertex_buffer;
    api.cmd_draw = cmd::draw;
    api.cmd_draw_indexed = cmd::draw_indexed;
    api.cmd_draw_indirect = cmd::draw_indirect;
    api.cmd_draw_indexed_indirect = cmd::draw_indexed_indirect;
    api.cmd_dispatch_workgroups = cmd::dispatch_workgroups;
    api.cmd_dispatch_workgroups_indirect = cmd::dispatch_workgroups_indirect;
    api.cmd_copy_buffer_to_buffer = cmd::copy_buffer_to_buffer;
    api.cmd_copy_buffer_to_texture = cmd::copy_buffer_to_texture;
    api.cmd_copy_texture_to_buffer = cmd::copy_texture_to_buffer;
    api.cmd_copy_texture_to_texture = cmd::copy_texture_to_texture;
    api.cmd_clear_buffer = cmd::clear_buffer;
    api.cmd_clear_texture = cmd::clear_texture;
    api.cmd_set_viewport = cmd::set_viewport;
    api.cmd_set_scissor_rect = cmd::set_scissor_rect;
    api.cmd_set_blend_constant = cmd::set_blend_constant;
    api.cmd_set_stencil_reference = cmd::set_stencil_reference;
    api.cmd_begin_occlusion_query = cmd::begin_occlusion_query;
    api.cmd_end_occlusion_query = cmd::end_occlusion_query;
    api.cmd_write_timestamp = cmd::write_timestamp;
    api.cmd_write_blas_properties = cmd::write_blas_properties;
    api.cmd_resolve_query_set = cmd::resolve_query_set;
    api.cmd_memory_barrier = cmd::memory_barrier;
    api.cmd_buffer_barrier = cmd::buffer_barrier;
    api.cmd_texture_barrier = cmd::texture_barrier;
    api.cmd_build_tlases = cmd::build_tlases;
    api.cmd_build_blases = cmd::build_blases;
    api.cmd_copy_blas = cmd::copy_blas;

    return api;
}
