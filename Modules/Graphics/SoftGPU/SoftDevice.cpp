#include "SoftDevice.h"

using namespace lyra;
using namespace lyra::softgpu;

static Own<SoftDevice> G_SOFT_DEVICE;

auto lyra::softgpu::get_device() -> SoftDevice*
{
    if (!G_SOFT_DEVICE) {
        G_SOFT_DEVICE = make_own<SoftDevice>();
    }
    return G_SOFT_DEVICE.get();
}

bool SoftDevice::init(const RHIDescriptor& descriptor)
{
    adapter_props.info.vendor      = "Lyra";
    adapter_props.info.device      = "Software Reference Renderer";
    adapter_props.info.description = "Lyra CPU Software RHI (Rasterization + Raytracing)";

    // capabilities
    adapter_props.features.raytracing = true;
    adapter_props.features.bindless   = true;

    // properties
    adapter_props.properties.subgroup_min_size            = 32;
    adapter_props.properties.subgroup_max_size            = 32;
    adapter_props.properties.texture_row_pitch_alignment  = 256;
    adapter_props.properties.min_uniform_buffer_alignment = 256;

    initialized.store(true, MemoryOrder::release);
    return true;
}

void SoftDevice::destroy()
{
    buffers              = {};
    textures             = {};
    texture_views        = {};
    samplers             = {};
    fences               = {};
    blases               = {};
    tlases               = {};
    pipeline_layouts     = {};
    bind_group_layouts   = {};
    bind_groups          = {};
    render_pipelines     = {};
    compute_pipelines    = {};
    raytracing_pipelines = {};
    swapchains           = {};
    query_sets           = {};

    initialized.store(false, MemoryOrder::release);
}
