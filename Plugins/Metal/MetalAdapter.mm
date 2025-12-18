#include "MetalUtils.h"

using namespace lyra;

// Metal adapter creation (select GPU device)
bool api::create_adapter(GPUAdapterProps& adapter, const GPUAdapterDescriptor& descriptor)
{
    auto rhi = get_rhi();

    // Get default Metal device
    rhi->device = MTLCreateSystemDefaultDevice();

    if (!rhi->device) {
        get_logger()->error("Failed to create Metal device");
        return false;
    }

    // Populate adapter properties
    adapter.info.description = [[rhi->device name] UTF8String];
    adapter.info.device = "Metal GPU";
    adapter.info.vendor = "Apple";
    adapter.info.architecture = "Metal";

    // Set supported features
    adapter.features.depth_clip_control = true;
    adapter.features.depth32float_stencil8 = true;
    adapter.features.timestamp_query = true;
    adapter.features.texture_compression_bc = true;
    adapter.features.indirect_first_instance = true;
    adapter.features.shader_f16 = true;
    adapter.features.bgra8unorm_storage = true;
    // adapter.features.pipeline_statistics_query = false;

    // Check for raytracing support (Metal 3+)
    if (@available(macOS 12.0, iOS 15.0, *)) {
        adapter.features.raytracing = [rhi->device supportsRaytracing];
    } else {
        adapter.features.raytracing = false;
    }

    // Set limits
    adapter.limits.max_texture_dimension_1d = 16384;
    adapter.limits.max_texture_dimension_2d = 16384;
    adapter.limits.max_texture_dimension_3d = 2048;
    adapter.limits.max_texture_array_layers = 2048;
    adapter.limits.max_bind_groups = 8;
    adapter.limits.max_bindings_per_bind_group = 31;
    adapter.limits.max_dynamic_uniform_buffers_per_pipeline_layout = 8;
    adapter.limits.max_dynamic_storage_buffers_per_pipeline_layout = 4;
    adapter.limits.max_sampled_textures_per_shader_stage = 31;
    adapter.limits.max_samplers_per_shader_stage = 16;
    adapter.limits.max_storage_buffers_per_shader_stage = 31;
    adapter.limits.max_storage_textures_per_shader_stage = 8;
    adapter.limits.max_uniform_buffers_per_shader_stage = 31;
    adapter.limits.max_uniform_buffer_binding_size = 64 * 1024;
    adapter.limits.max_storage_buffer_binding_size = 1024 * 1024 * 1024;
    adapter.limits.max_buffer_size = 1024 * 1024 * 1024;
    adapter.limits.max_vertex_buffers = 31;
    adapter.limits.max_vertex_attributes = 31;
    adapter.limits.max_vertex_buffer_array_stride = 2048;
    adapter.limits.min_uniform_buffer_offset_alignment = 256;
    adapter.limits.min_storage_buffer_offset_alignment = 16;
    adapter.limits.max_compute_workgroup_size_x = 1024;
    adapter.limits.max_compute_workgroup_size_y = 1024;
    adapter.limits.max_compute_workgroup_size_z = 1024;
    adapter.limits.max_compute_invocations_per_workgroup = 1024;
    adapter.limits.max_compute_workgroups_per_dimension = 65535;

    get_logger()->info("Metal adapter created: {}", adapter.info.description);
    return true;
}

void api::delete_adapter()
{
    // Metal adapter (MTLDevice selection) does not need cleanup
    // The device is released in delete_device
}
