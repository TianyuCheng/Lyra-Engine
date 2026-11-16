#pragma once

#ifndef LYRA_MODULES_GRAPHICS_SOFTWARE_SOFTPIPELINE_H
#define LYRA_MODULES_GRAPHICS_SOFTWARE_SOFTPIPELINE_H

#include "SoftCommon.h"
#include "SoftBuffer.h"
#include "SoftTexture.h"
#include "SoftSampler.h"
#include "SoftRaytracing.h"

namespace lyra::softgpu
{
    struct SoftPipelineLayout
    {
        Vector<GPUBindGroupLayoutHandle> bind_group_layouts;
        uint32_t                         immediate_size = 0;
        CString                          label          = "";

        SoftPipelineLayout() = default;
        explicit SoftPipelineLayout(const GPUPipelineLayoutDescriptor& desc);
    };

    struct SoftBindGroupLayout
    {
        GPUBindGroupLayoutDescriptor desc;
        CString                      label = "";

        SoftBindGroupLayout() = default;
        explicit SoftBindGroupLayout(const GPUBindGroupLayoutDescriptor& descriptor);
    };

    struct SoftBindGroup
    {
        GPUBindGroupDescriptor    desc;
        Vector<GPUBindGroupEntry> entries;
        CString                   label = "";

        SoftBindGroup() = default;
        explicit SoftBindGroup(const GPUBindGroupDescriptor& descriptor);

        auto find_entry(uint32_t binding) const -> const GPUBindGroupEntry*;
    };

    struct SoftShaderModule
    {
        Vector<uint8_t> bytecode;
        void*           host_library = nullptr;
        void*           host_func    = nullptr;
        CString         label        = "";
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

        GPUShaderModuleHandle fshader_module;
        void*                 vshader_func = nullptr;
        void*                 fshader_func = nullptr;

        SoftRenderPipeline() = default;
        explicit SoftRenderPipeline(const GPURenderPipelineDescriptor& desc);
    };

    struct SoftComputePipeline
    {
        GPUPipelineLayoutHandle layout;
        GPUProgrammableStage    compute;
        CString                 label = "";

        void* cshader_func = nullptr;

        SoftComputePipeline() = default;
        explicit SoftComputePipeline(const GPUComputePipelineDescriptor& desc);
    };

    struct SoftRayTracingPipeline
    {
        GPUPipelineLayoutHandle layout;
        uint32_t                max_recursion_depth = 5;
        CString                 label               = "";

        SoftRayTracingPipeline() = default;
        explicit SoftRayTracingPipeline(const GPURayTracingPipelineDescriptor& desc);
    };
} // namespace lyra::softgpu

#endif // LYRA_MODULES_GRAPHICS_SOFTWARE_SOFTPIPELINE_H
