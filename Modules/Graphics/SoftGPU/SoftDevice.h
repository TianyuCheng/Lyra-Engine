#pragma once

#ifndef LYRA_MODULES_GRAPHICS_SOFTWARE_SOFTDEVICE_H
#define LYRA_MODULES_GRAPHICS_SOFTWARE_SOFTDEVICE_H

#include "SoftCommon.h"
#include "SoftBuffer.h"
#include "SoftTexture.h"
#include "SoftSampler.h"
#include "SoftFence.h"
#include "SoftSwapchain.h"
#include "SoftRaytracing.h"
#include "SoftPipeline.h"

namespace lyra::softgpu
{
    struct SoftQuerySet
    {
        GPUQueryType     type;
        GPUSize32        count;
        Vector<uint64_t> results;
        CString          label = "";
    };

    struct SoftDevice
    {
        GPUAdapterProps   adapter_props;
        std::atomic<bool> initialized{false};

        Slotmap<SoftBuffer>             buffers;
        Slotmap<SoftTexture>            textures;
        Slotmap<SoftTextureView>        texture_views;
        Slotmap<SoftSampler>            samplers;
        Slotmap<SoftFence>              fences;
        Slotmap<SoftBlas>               blases;
        Slotmap<SoftTlas>               tlases;
        Slotmap<SoftPipelineLayout>     pipeline_layouts;
        Slotmap<SoftBindGroupLayout>    bind_group_layouts;
        Slotmap<SoftBindGroup>          bind_groups;
        Slotmap<SoftShaderModule>       shader_modules;
        Slotmap<SoftRenderPipeline>     render_pipelines;
        Slotmap<SoftComputePipeline>    compute_pipelines;
        Slotmap<SoftRayTracingPipeline> raytracing_pipelines;
        Slotmap<SoftSwapchain>          swapchains;
        Slotmap<SoftQuerySet>           query_sets;

        SoftDevice() = default;

        auto init(const RHIDescriptor& descriptor) -> bool;
        auto destroy() -> void;
    };

    auto get_device() -> SoftDevice*;
} // namespace lyra::softgpu

#endif // LYRA_MODULES_GRAPHICS_SOFTWARE_SOFTDEVICE_H
