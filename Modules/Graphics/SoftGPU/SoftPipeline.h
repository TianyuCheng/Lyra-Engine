#pragma once

#ifndef LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTPIPELINE_H
#define LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTPIPELINE_H

#include "SoftCommon.h"
#include "SoftBuffer.h"
#include "SoftTexture.h"
#include "SoftSampler.h"
#include "SoftRaytracing.h"

#include <Lyra/Compiler/SLCUtils.h>

namespace lyra::softgpu
{
    struct SoftPipelineLayout
    {
        Vector<GPUBindGroupLayoutHandle> bind_group_layouts;
        uint                             immediate_size = 0;
        CString                          label          = "";

        SoftPipelineLayout() = default;
        explicit SoftPipelineLayout(const GPUPipelineLayoutDescriptor& desc);

        void destroy() {}
    };

    struct SoftBindGroupLayout
    {
        GPUBindGroupLayoutDescriptor desc;
        CString                      label = "";

        SoftBindGroupLayout() = default;
        explicit SoftBindGroupLayout(const GPUBindGroupLayoutDescriptor& descriptor);

        void destroy() {}
    };

    struct SoftBindGroup
    {
        GPUBindGroupDescriptor    desc;
        Vector<GPUBindGroupEntry> entries;
        CString                   label = "";

        SoftBindGroup() = default;
        explicit SoftBindGroup(const GPUBindGroupDescriptor& descriptor);

        const GPUBindGroupEntry* find_entry(uint binding) const;

        void destroy() {}
    };

    struct SoftShaderModule
    {
        Vector<ubyte>    bytecode;
        void*            host_library = nullptr;
        void*            host_func    = nullptr;
        CString          label        = "";
        char             name[64]{};
        uint             param_count = 0;
        HostParamBinding params[32]{};
        ushort           depth_offset = 0xFFFF;

        void destroy() {}
    };

    struct SoftVertexAttribute
    {
        GPUVertexFormat format          = GPUVertexFormat::FLOAT32x3;
        GPUSize64       offset          = 0;
        GPUIndex32      shader_location = 0;
    };

    struct SoftVertexBufferLayout
    {
        GPUSize64                   array_stride = 0;
        GPUVertexStepMode           step_mode    = GPUVertexStepMode::VERTEX;
        Vector<SoftVertexAttribute> attributes;
    };

    struct SoftRenderPipeline
    {
        GPUPipelineLayoutHandle        layout;
        Vector<SoftVertexBufferLayout> vertex_buffers;
        Vector<GPUColorTargetState>    color_targets;
        GPUPrimitiveState              primitive;
        GPUDepthStencilState           depth_stencil;
        GPUMultisampleState            multisample;
        CString                        label = "";

        GPUShaderModuleHandle vshader_module;
        GPUShaderModuleHandle fshader_module;
        String                ventry_point;
        String                fentry_point;
        void*                 vshader_func = nullptr;
        void*                 fshader_func = nullptr;
        ushort                depth_offset = 0xFFFF;

        SoftRenderPipeline() = default;
        explicit SoftRenderPipeline(const GPURenderPipelineDescriptor& desc);

        void destroy() {}
    };

    struct SoftComputePipeline
    {
        GPUPipelineLayoutHandle layout;
        GPUProgrammableStage    compute;
        CString                 label = "";

        void* cshader_func = nullptr;

        SoftComputePipeline() = default;
        explicit SoftComputePipeline(const GPUComputePipelineDescriptor& desc);

        void destroy() {}
    };

    struct SoftRayTracingPipeline
    {
        GPUPipelineLayoutHandle layout;
        uint                    max_recursion_depth = 5;
        CString                 label               = "";

        SoftRayTracingPipeline() = default;
        explicit SoftRayTracingPipeline(const GPURayTracingPipelineDescriptor& desc);

        void destroy() {}
    };
} // namespace lyra::softgpu

#endif // LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTPIPELINE_H
