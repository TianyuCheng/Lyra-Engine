#pragma once

#ifndef LYRA_LIBRARY_RENDER_RHI_DESCS_H
#define LYRA_LIBRARY_RENDER_RHI_DESCS_H

#include <Lyra/Window/WSIUtils.h>
#include <Lyra/Render/RHI/RHIEnums.h>
#include <Lyra/Render/RHI/RHIUtils.h>

namespace lyra
{
    struct GPUObjectDescriptorBase
    {
        CString label = "";
    };

    struct RHIDescriptor : public GPUObjectDescriptorBase
    {
        RHIFlags     flags = 0;
        RHIBackend   backend;
        WindowHandle window = {};
    };

    struct GPUAdapterDescriptor : public GPUObjectDescriptorBase
    {
        // nothing here for now
    };

    struct GPUDeviceDescriptor : public GPUObjectDescriptorBase
    {
        GPUFeatureNames required_features = {};
    };

    struct GPUSurfaceDescriptor : public GPUObjectDescriptorBase
    {
        WindowHandle window = {};

        GPUCompositeAlphaMode alpha_mode   = GPUCompositeAlphaMode::Opaque;
        GPUPresentMode        present_mode = GPUPresentMode::Fifo;
        GPUColorSpace         color_space  = GPUColorSpace::SRGB;
        uint                  frames       = 3;
    };

    struct GPUQueueDescriptor : public GPUObjectDescriptorBase
    {
        // nothing here for now
    };

    struct GPUBufferDescriptor : public GPUObjectDescriptorBase
    {
        GPUSize64           size               = 0;
        GPUBufferUsageFlags usage              = 0;
        bool                virtual_address    = false;
        bool                mapped_at_creation = false;

        friend bool operator==(const GPUBufferDescriptor& lhs, const GPUBufferDescriptor& rhs)
        {
            return lhs.size == rhs.size &&
                   lhs.usage.value == rhs.usage &&
                   lhs.virtual_address == rhs.virtual_address;
        }
    };

    struct GPUSamplerDescriptor : public GPUObjectDescriptorBase
    {
        GPUAddressMode      address_mode_u = GPUAddressMode::CLAMP_TO_EDGE;
        GPUAddressMode      address_mode_v = GPUAddressMode::CLAMP_TO_EDGE;
        GPUAddressMode      address_mode_w = GPUAddressMode::CLAMP_TO_EDGE;
        GPUFilterMode       mag_filter     = GPUFilterMode::NEAREST;
        GPUFilterMode       min_filter     = GPUFilterMode::NEAREST;
        GPUMipmapFilterMode mipmap_filter  = GPUMipmapFilterMode::NEAREST;
        float               lod_min_clamp  = 0.0f;
        float               lod_max_clamp  = 32.0f;
        GPUCompareFunction  compare        = GPUCompareFunction::GREATER;
        uint                max_anisotropy = 1u;
        bool                compare_enable = false;
    };

    struct GPUTextureDescriptor : public GPUObjectDescriptorBase
    {
        GPUExtent3D          size;
        GPUIntegerCoordinate mip_level_count = 1;
        GPUIntegerCoordinate array_layers    = 1;
        GPUSize32            sample_count    = 1;
        GPUTextureDimension  dimension       = GPUTextureDimension::x2D;
        GPUTextureFormat     format          = GPUTextureFormat::RGBA8UNORM;
        GPUTextureUsageFlags usage           = 0;

        friend bool operator==(const GPUTextureDescriptor& lhs, const GPUTextureDescriptor& rhs)
        {
            return lhs.size.width == rhs.size.width &&
                   lhs.size.height == rhs.size.height &&
                   lhs.size.depth == rhs.size.depth &&
                   lhs.mip_level_count == rhs.mip_level_count &&
                   lhs.array_layers == rhs.array_layers &&
                   lhs.sample_count == rhs.sample_count &&
                   lhs.dimension == rhs.dimension &&
                   lhs.format == rhs.format &&
                   lhs.usage.value == rhs.usage;
        }
    };

    struct GPUTextureViewDescriptor : public GPUObjectDescriptorBase
    {
        GPUTextureFormat        format;
        GPUTextureViewDimension dimension;
        GPUTextureUsageFlags    usage             = 0; // if specified as 0, use texture's usage
        GPUTextureAspectFlags   aspect            = GPUTextureAspect::ALL;
        GPUIntegerCoordinate    base_mip_level    = 0;
        GPUIntegerCoordinate    mip_level_count   = 1;
        GPUIntegerCoordinate    base_array_layer  = 0;
        GPUIntegerCoordinate    array_layer_count = 1;
    };

    struct GPUShaderModuleDescriptor : public GPUObjectDescriptorBase
    {
        uint8_t* data = nullptr;
        uint     size = 0;
    };

    struct GPUQuerySetDescriptor : public GPUObjectDescriptorBase
    {
        GPUQueryType type;
        GPUSize32    count;
    };

    // NOTE: Non-WebGPU standard API
    struct GPUBlasDescriptor : public GPUObjectDescriptorBase
    {
        GPUBVHFlags      flags       = 0;
        GPUBVHUpdateMode update_mode = GPUBVHUpdateMode::BUILD;
    };

    // NOTE: Non-WebGPU standard API
    struct GPUTlasDescriptor : public GPUObjectDescriptorBase
    {
        uint             max_instances = 0;
        GPUBVHFlags      flags         = 0;
        GPUBVHUpdateMode update_mode   = GPUBVHUpdateMode::BUILD;
    };

    // NOTE: Non-WebGPU standard API
    struct GPUBindGroupHeapDescriptor : public GPUObjectDescriptorBase
    {
        uint page_size = 2048;
    };

    struct GPUBindGroupDescriptor : public GPUObjectDescriptorBase
    {
        GPUBindGroupHeapHandle   heap; // NOTE: Non-WebGPU standard API
        GPUBindGroupLayoutHandle layout;
        GPUBindGroupEntries      entries;
    };

    struct GPUBindGroupLayoutDescriptor : public GPUObjectDescriptorBase
    {
        GPUBindGroupLayoutEntries entries = {};
    };

    struct GPUPipelineLayoutDescriptor : public GPUObjectDescriptorBase
    {
        GPUBindGroupLayoutHandles bind_group_layouts;
        GPUPushConstantRanges     push_constant_ranges;
    };

    struct GPUPipelineDescriptorBase : public GPUObjectDescriptorBase
    {
        GPUPipelineLayoutHandle layout;
    };

    struct GPUComputePipelineDescriptor : public GPUPipelineDescriptorBase
    {
        GPUProgrammableStage compute;
    };

    struct GPURenderPipelineDescriptor : public GPUPipelineDescriptorBase
    {
        GPUVertexState       vertex        = {};
        GPUPrimitiveState    primitive     = {};
        GPUDepthStencilState depth_stencil = {};
        GPUMultisampleState  multisample   = {};
        GPUFragmentState     fragment      = {};
    };

    struct GPURayTracingPipelineDescriptor : public GPUPipelineDescriptorBase
    {
        uint max_recursion_depth = 5;
    };

    struct GPURenderPassLayout : public GPUObjectDescriptorBase
    {
        GPUTextureFormats color_formats;
        GPUTextureFormat  depth_stencil_format;
        GPUSize32         sample_count = 1;
    };

    struct GPURenderPassDescriptor : public GPUObjectDescriptorBase
    {
        GPURenderPassColorAttachments       color_attachments;
        GPURenderPassDepthStencilAttachment depth_stencil_attachment;
        GPUQuerySetHandle                   occlusion_query_set;
        GPUSize64                           max_draw_count = 50000000;
    };

    struct GPUCommandBufferDescriptor : public GPUObjectDescriptorBase
    {
        GPUQueueType queue = GPUQueueType::DEFAULT;
    };

    struct GPUCommandBundleDescriptor : public GPUObjectDescriptorBase
    {
        GPUQueueType queue = GPUQueueType::DEFAULT;
    };

} // namespace lyra

#endif // LYRA_LIBRARY_RENDER_RHI_DESCS_H
