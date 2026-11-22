#include "SoftDevice.h"
#include "SoftCommandBuffer.h"

using namespace lyra;
using namespace lyra::softgpu;

static Own<SoftDevice> G_SOFT_DEVICE;

SoftDevice* lyra::softgpu::get_device()
{
    if (!G_SOFT_DEVICE) {
        G_SOFT_DEVICE = make_own<SoftDevice>();
    }
    return G_SOFT_DEVICE.get();
}

bool SoftDevice::init(const RHIDescriptor& descriptor)
{
    get_logger()->info("Initializing SoftGPU Reference Renderer");

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
    get_logger()->trace("Destroying SoftGPU device resources");

    command_buffers.clear();

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

SoftCommandBuffer& SoftDevice::command(GPUCommandEncoderHandle handle)
{
    auto it = command_buffers.find(handle.value);
    if (it == command_buffers.end()) {
        auto buf = make_own<SoftCommandBuffer>();
        it       = command_buffers.emplace(handle.value, std::move(buf)).first;
    }
    return *it->second;
}
