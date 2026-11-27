#pragma once

#ifndef LYRA_ENGINE_GRAPHICS_RHIUTILS_H
#define LYRA_ENGINE_GRAPHICS_RHIUTILS_H

#include <Lyra/Utilities/Assert.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Handle.h>
#include <Lyra/Utilities/BitFlags.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Graphics/RHIEnums.h>

ENABLE_BIT_FLAGS(lyra::RHIFlag);
ENABLE_BIT_FLAGS(lyra::GPUShaderStage);
ENABLE_BIT_FLAGS(lyra::GPUBufferUsage);
ENABLE_BIT_FLAGS(lyra::GPUTextureUsage);
ENABLE_BIT_FLAGS(lyra::GPUColorWrite);
ENABLE_BIT_FLAGS(lyra::GPUBarrierSync);
ENABLE_BIT_FLAGS(lyra::GPUBarrierAccess);
ENABLE_BIT_FLAGS(lyra::GPUTextureAspect);
ENABLE_BIT_FLAGS(lyra::GPUBVHFlag);
ENABLE_BIT_FLAGS(lyra::GPUBVHGeometryFlag);

namespace lyra
{
    // NOTE: D3D12 uses root constants to implement immediates,
    // but root constant is similar to other descriptor table based
    // bind group layouts that requires both a register space, and
    // a base register. For simplicity, we force that immediates
    // always use space999 (max).
    static constexpr uint D3D12_ImmediateRegisterSpace = 999;

    // NOTE: Metal uses buffer to implement immediates,
    // without explicit annotation the immediate buffer could
    // be at any buffer index, causing additional difficulty to track
    // the buffer index for other regular buffers. For simplicity,
    // we force that immediates always use buffer(30) (max)
    static constexpr uint METAL_ImmediateBufferIndex = 30;

    // NOTE: Metal binds vertex attributes in the same space as
    // regular buffers. Therefore as a workaround, we bind the vertex
    // buffers in the high buffer slots backwardly to avoid slot collision
    // with other buffer bindings.
    static constexpr uint METAL_VertexBufferSlotIndex = 29;

    using BufferSource = uint8_t*;

    using GPUBufferDynamicOffset   = uint;
    using GPUStencilValue          = uint;
    using GPUSampleMask            = uint;
    using GPUDepthBias             = int32_t;
    using GPUSize64                = ulong;
    using GPUIntegerCoordinate     = uint;
    using GPUIndex32               = uint;
    using GPUSize32                = uint;
    using GPUSignedOffset32        = int32_t;
    using GPUIndex64               = ulong;
    using GPUSize64Out             = ulong;
    using GPUIntegerCoordinateOut  = uint;
    using GPUSize32Out             = uint;
    using GPUFlagsConstant         = uint;
    using GPUPipelineConstantValue = ulong;
    using GPUBufferAddress         = ulong;
    using RHIFlags                 = BitFlags<RHIFlag>;
    using GPUShaderStageFlags      = BitFlags<GPUShaderStage>;
    using GPUBufferUsageFlags      = BitFlags<GPUBufferUsage>;
    using GPUTextureUsageFlags     = BitFlags<GPUTextureUsage>;
    using GPUColorWriteFlags       = BitFlags<GPUColorWrite>;
    using GPUBarrierSyncFlags      = BitFlags<GPUBarrierSync>;
    using GPUBarrierAccessFlags    = BitFlags<GPUBarrierAccess>;
    using GPUBVHFlags              = BitFlags<GPUBVHFlag>;
    using GPUBVHGeometryFlags      = BitFlags<GPUBVHGeometryFlag>;
    using GPUTextureAspectFlags    = BitFlags<GPUTextureAspect>;

    inline String to_string(GPUColorWriteFlags mask)
    {
        if (mask == GPUColorWrite::ALL) return "all";
        if (mask.value == 0) return "none";

        String s;
        if (mask.contains(GPUColorWrite::RED)) s += "r";
        if (mask.contains(GPUColorWrite::GREEN)) s += "g";
        if (mask.contains(GPUColorWrite::BLUE)) s += "b";
        if (mask.contains(GPUColorWrite::ALPHA)) s += "a";
        return s;
    }

    template <GPUObjectType E>
    struct GPUHandle : public TypedEnumHandle<GPUObjectType, E, GPUIndex64>
    {
        static constexpr ulong RHI_MASK = 0xF000'0000'0000'0000ULL;
        static constexpr ulong RAW_MASK = 0x0FFF'FFFF'FFFF'FFFFULL;

        using Base = TypedEnumHandle<GPUObjectType, E, GPUIndex64>;
        using Base::Base;
        using Base::value;

        FORCE_INLINE bool valid() const
        {
            return value != GPUIndex64(-1) && (static_cast<ulong>(value) & RAW_MASK) != RAW_MASK;
        }

        FORCE_INLINE ubyte rhi() const
        {
            return static_cast<ubyte>((static_cast<ulong>(value) >> 60) & 0x0F);
        }

        FORCE_INLINE GPUHandle raw() const
        {
            return GPUHandle(static_cast<GPUIndex64>(static_cast<ulong>(value) & RAW_MASK));
        }

        FORCE_INLINE static GPUHandle pack(GPUHandle handle, ubyte rhi)
        {
            ulong packed = (static_cast<ulong>(handle.value) & RAW_MASK) | (static_cast<ulong>(rhi & 0x0F) << 60);
            return GPUHandle(static_cast<GPUIndex64>(packed));
        }

        template <typename SlotType>
        FORCE_INLINE static GPUHandle create(apus::slot_map_handle<SlotType> handle)
        {
            return GPUHandle(as_type<GPUIndex64>(handle));
        }

        template <typename SlotType>
        FORCE_INLINE apus::slot_map_handle<SlotType> to_slotmap_handle() const
        {
            ulong raw_val = static_cast<ulong>(value) & RAW_MASK;
            return as_type<apus::slot_map_handle<SlotType>>(raw_val);
        }
    };

    // typed GPU handle
    using GPUSurfaceHandle            = GPUHandle<GPUObjectType::SURFACE>;
    using GPUFenceHandle              = GPUHandle<GPUObjectType::FENCE>;
    using GPUCommandEncoderHandle     = GPUHandle<GPUObjectType::COMMAND_ENCODER>;
    using GPUBufferHandle             = GPUHandle<GPUObjectType::BUFFER>;
    using GPUSamplerHandle            = GPUHandle<GPUObjectType::SAMPLER>;
    using GPUTextureHandle            = GPUHandle<GPUObjectType::TEXTURE>;
    using GPUTextureViewHandle        = GPUHandle<GPUObjectType::TEXTURE_VIEW>;
    using GPUShaderModuleHandle       = GPUHandle<GPUObjectType::SHADER_MODULE>;
    using GPUQuerySetHandle           = GPUHandle<GPUObjectType::QUERY_SET>;
    using GPUTlasHandle               = GPUHandle<GPUObjectType::TLAS>;
    using GPUBlasHandle               = GPUHandle<GPUObjectType::BLAS>;
    using GPUBindGroupHandle          = GPUHandle<GPUObjectType::BIND_GROUP>;
    using GPUBindGroupHeapHandle      = GPUHandle<GPUObjectType::BIND_GROUP_HEAP>;
    using GPUBindGroupLayoutHandle    = GPUHandle<GPUObjectType::BIND_GROUP_LAYOUT>;
    using GPUPipelineLayoutHandle     = GPUHandle<GPUObjectType::PIPELINE_LAYOUT>;
    using GPURenderPipelineHandle     = GPUHandle<GPUObjectType::RENDER_PIPELINE>;
    using GPUComputePipelineHandle    = GPUHandle<GPUObjectType::COMPUTE_PIPELINE>;
    using GPURayTracingPipelineHandle = GPUHandle<GPUObjectType::RAYTRACING_PIPELINE>;

    // forward declarations
    using GPUFeatureNames   = Span<GPUFeatureName>;
    using GPUTextureFormats = Span<GPUTextureFormat>;

    struct GPUTlasInstance;
    using GPUTlasInstances = Span<GPUTlasInstance>;

    struct GPUBlasTriangleGeometry;
    using GPUBlasTriangleGeometries = Span<GPUBlasTriangleGeometry>;

    struct GPUBindGroupEntry;
    using GPUBindGroupEntries = Span<GPUBindGroupEntry>;

    struct GPUBindGroupLayoutEntry;
    using GPUBindGroupLayoutEntries = Span<GPUBindGroupLayoutEntry>;
    using GPUBindGroupLayoutHandles = Span<GPUBindGroupLayoutHandle>;

    struct GPUBindGroupLayoutDescriptor;
    using GPUBindGroupLayoutDescriptors = Span<GPUBindGroupLayoutDescriptor>;

    struct GPUPipelineConstant;
    using GPUPipelineConstants = Span<GPUPipelineConstant>;

    struct GPUVertexAttribute;
    using GPUVertexAttributes = Span<GPUVertexAttribute>;

    struct GPUVertexBufferLayout;
    using GPUVertexBufferLayouts = Span<GPUVertexBufferLayout>;

    struct GPUColorTargetState;
    using GPUColorTargetStates = Span<GPUColorTargetState>;

    struct GPURenderPassColorAttachment;
    using GPURenderPassColorAttachments = Span<GPURenderPassColorAttachment>;

    struct GPUBlasGeometrySizeDescriptor;
    using GPUBlasGeometrySizeDescriptors = Span<GPUBlasGeometrySizeDescriptor>;

    struct GPUTlasBuildEntry;
    using GPUTlasBuildEntries = Span<GPUTlasBuildEntry>;

    struct GPUBlasBuildEntry;
    using GPUBlasBuildEntries = Span<GPUBlasBuildEntry>;

    struct GPUMemoryBarrier;
    using GPUMemoryBarriers = Span<GPUMemoryBarrier>;

    struct GPUBufferBarrier;
    using GPUBufferBarriers = Span<GPUBufferBarrier>;

    struct GPUTextureBarrier;
    using GPUTextureBarriers = Span<GPUTextureBarrier>;

    struct GPUBufferBinding;
    using GPUBufferBindings     = Span<GPUBufferBinding>;
    using GPUSamplerHandles     = Span<GPUSamplerHandle>;
    using GPUTextureViewHandles = Span<GPUTextureViewHandle>;

    using GPUBufferDynamicOffsets = Span<GPUBufferDynamicOffset>;

    struct MappedBufferRange
    {
        BufferSource data;
        usize        size;
    };

    template <typename T>
    struct TypedBufferRange
    {
        T*    data;
        usize count;

        auto operator[](usize i) -> T&
        {
            return data[i];
        }

        auto operator[](usize i) const -> T&
        {
            return data[i];
        }

        auto at(usize i) -> T&
        {
            assert(i < count);
            return data[i];
        }

        auto at(usize i) const -> T&
        {
            assert(i < count);
            return data[i];
        }
    };

    // NOTE: Non-WebGPU standard because we support multiple RHI backends,
    // not all RHI backends support the binding model in the same way as WebGPU.
    struct GPUBindingIndex
    {
        // NOTE: common property shared among all RHI backends.
        // for Vulkan: flattened binding index (unique across the bind groupgroupgroup)
        // for D3D12: space-based register index (different spaces might have same index binding)
        // for Metal: space-based binding index
        // While this become cumbersome for users to maintain across different platforms,
        // it is recommended to use the reflection API to populate it.
        ushort index = 0;

        // NOTE: Metal differs from Vulkan/D3D12 that it has support for both
        // 1. directly bound resources via resource slot
        // 2. indirectly bound resources via argument buffer.
        // since a resource can be bound in either way, simply using the binding index
        // does not suffice to differentiate between the binding mode. We need this extra
        // bit of information to tell how we can bind it.
        bool from_argument_buffer = false;
    };

    struct GPUSupportedFeatures
    {
        bool bgra8unorm_storage                 = false;
        bool clip_distances                     = false;
        bool depth_clip_control                 = false;
        bool depth32float_stencil8              = false;
        bool dual_source_blending               = false;
        bool float32_blendable                  = false;
        bool float32_filterable                 = false;
        bool indirect_first_instance            = false;
        bool rg11b10ufloat_renderable           = false;
        bool shader_f16                         = false;
        bool subgroups                          = false;
        bool texture_compression_bc             = false;
        bool texture_compression_bc_sliced_3d   = false;
        bool texture_compression_astc           = false;
        bool texture_compression_astc_sliced_3d = false;
        bool texture_compression_etc2           = false;
        bool timestamp_query                    = false;
        bool bindless                           = false;
        bool raytracing                         = false;
    };

    struct GPUSupportedLimits
    {
        uint max_texture_dimension_1d                        = 8192;
        uint max_texture_dimension_2d                        = 8192;
        uint max_texture_dimension_3d                        = 2048;
        uint max_texture_array_layers                        = 256;
        uint max_bind_groups                                 = 4;
        uint max_bindings_per_bind_group                     = 640;
        uint max_dynamic_uniform_buffers_per_pipeline_layout = 8;
        uint max_dynamic_storage_buffers_per_pipeline_layout = 4;
        uint max_sampled_textures_per_shader_stage           = 16;
        uint max_samplers_per_shader_stage                   = 16;
        uint max_storage_buffers_per_shader_stage            = 8;
        uint max_storage_textures_per_shader_stage           = 4;
        uint max_uniform_buffers_per_shader_stage            = 12;
        uint max_uniform_buffer_binding_size                 = 65536;
        uint max_storage_buffer_binding_size                 = 134217728;
        uint min_uniform_buffer_offset_alignment             = 256;
        uint min_storage_buffer_offset_alignment             = 256;
        uint max_vertex_buffers                              = 8;
        uint max_buffer_size                                 = 268435456;
        uint max_vertex_attributes                           = 16;
        uint max_vertex_buffer_array_stride                  = 2048;
        uint max_inter_stage_shader_variables                = 16;
        uint max_color_attachments                           = 8;
        uint max_color_attachment_bytes_per_sample           = 32;
        uint max_compute_workgroup_storage_size              = 16384;
        uint max_compute_invocations_per_workgroup           = 256;
        uint max_compute_workgroup_size_x                    = 256;
        uint max_compute_workgroup_size_y                    = 256;
        uint max_compute_workgroup_size_z                    = 64;
        uint max_compute_workgroups_per_dimension            = 65535;
        uint max_immediate_size                              = 128;
    };

    constexpr uint GPU_IMMEDIATE_DATA_ALIGNMENT = 4;

    struct GPUProperties
    {
        uint subgroup_max_size            = 0;
        uint subgroup_min_size            = 0;
        uint texture_row_pitch_alignment  = 0;
        uint min_uniform_buffer_alignment = 0;
    };

    struct GPUAdapterInfo
    {
        FixedString64 architecture = {};
        FixedString64 description  = {};
        FixedString64 device       = {};
        FixedString64 vendor       = {};
    };

    static_assert(std::is_standard_layout_v<GPUAdapterInfo>);
    static_assert(sizeof(GPUAdapterInfo) == 256);

    struct GPUAdapterProps
    {
        GPUAdapterInfo       info       = {};
        GPUSupportedFeatures features   = {};
        GPUSupportedLimits   limits     = {};
        GPUProperties        properties = {};
    };

    struct GPUColor
    {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        float a = 0.0f;
    };

    struct GPUOrigin2D
    {
        GPUIntegerCoordinate x = 0;
        GPUIntegerCoordinate y = 0;
    };

    struct GPUOrigin3D
    {
        GPUIntegerCoordinate x = 0;
        GPUIntegerCoordinate y = 0;
        GPUIntegerCoordinate z = 0;
    };

    struct GPUExtent2D
    {
        GPUIntegerCoordinate width  = 0;
        GPUIntegerCoordinate height = 0;
    };

    struct GPUExtent3D
    {
        GPUIntegerCoordinate width  = 0;
        GPUIntegerCoordinate height = 0;
        GPUIntegerCoordinate depth  = 0;
    };

    struct GPUTexelCopyBufferLayout
    {
        GPUSize64 offset         = 0;
        GPUSize32 bytes_per_row  = 0; // bytes_per_row = 0 indicates tightly packed texture
        GPUSize32 rows_per_image = 0;
    };

    struct GPUTexelCopyBufferInfo : GPUTexelCopyBufferLayout
    {
        GPUBufferHandle buffer;
    };

    struct GPUTexelCopyTextureInfo
    {
        GPUTextureHandle      texture;
        GPUIntegerCoordinate  mip_level = 0;
        GPUOrigin3D           origin    = {};
        GPUTextureAspectFlags aspect    = GPUTextureAspect::ALL;
    };

    struct GPUBufferBindingLayout
    {
        GPUBufferBindingType type               = GPUBufferBindingType::UNIFORM;
        bool                 has_dynamic_offset = false;
        GPUSize64            min_binding_size   = 0;
    };

    struct GPUSamplerBindingLayout
    {
        GPUSamplerBindingType type = GPUSamplerBindingType::FILTERING;
    };

    struct GPUTextureBindingLayout
    {
        GPUTextureSampleType    sample_type    = GPUTextureSampleType::FLOAT;
        GPUTextureViewDimension view_dimension = GPUTextureViewDimension::x2D;
        bool                    multisampled   = false;
    };

    struct GPUStorageTextureBindingLayout
    {
        GPUStorageTextureAccess access = GPUStorageTextureAccess::WRITE_ONLY;
        GPUTextureFormat        format;
        GPUTextureViewDimension view_dimension = GPUTextureViewDimension::x2D;
    };

    // NOTE: Non-WebGPU standard because WebGPU does not support raytracing.
    struct GPUBVHBindingLayout
    {
        bool vertex_return = false;
    };

    // NOTE: Non-WebGPU standard because WebGPU does not support raytracing.
    struct GPUBVHSizes
    {
        uint bvh_size;
        uint build_size;
        uint update_size;
    };

    // NOTE: Non-WebGPU standard because WebGPU does not support raytracing.
    struct GPUBlasTriangleGeometrySizeDescriptor
    {
        GPUVertexFormat     vertex_format;
        GPUIndexFormat      index_format;
        uint                vertex_count;
        uint                index_count;
        GPUBVHGeometryFlags flags;
    };

    // NOTE: Non-WebGPU standard because WebGPU does not support raytracing.
    struct GPUBlasGeometrySizeDescriptor
    {
        GPUBlasType type = GPUBlasType::TRIANGLE;
        union
        {
            GPUBlasTriangleGeometrySizeDescriptor triangles = {};
        };

        // default trivial constructor / destructor
        GPUBlasGeometrySizeDescriptor() {}
        ~GPUBlasGeometrySizeDescriptor() {}
    };

    // NOTE: Non-WebGPU standard because WebGPU does not support raytracing.
    // NOTE: index buffer is optional
    struct GPUBlasTriangleGeometry
    {
        GPUBlasTriangleGeometrySizeDescriptor size;
        GPUBufferHandle                       vertex_buffer;
        GPUBufferHandle                       index_buffer;
        GPUBufferHandle                       transform_buffer;
        uint                                  first_vertex;
        uint                                  first_index;
        GPUBufferAddress                      vertex_stride;
        GPUBufferAddress                      transform_buffer_offset;
    };

    // NOTE: Non-WebGPU standard because WebGPU does not support raytracing.
    struct GPUBlasGeometries
    {
        GPUBlasType type = GPUBlasType::TRIANGLE;
        union
        {
            GPUBlasTriangleGeometries triangles = {};
        };

        // default trivial constructor / destructor
        GPUBlasGeometries() {}
        ~GPUBlasGeometries() {}
    };

    // NOTE: Non-WebGPU standard because WebGPU does not support raytracing.
    struct GPUBlasBuildEntry
    {
        GPUBlasHandle     blas;
        GPUBlasGeometries geometries;
    };

    // NOTE: Non-WebGPU standard because WebGPU does not support raytracing.
    struct GPUTlasInstance
    {
        float         transform[3][4]; // row major 3x4
        uint          custom_data;
        ubyte         mask;
        GPUBlasHandle blas;
    };

    // NOTE: Non-WebGPU standard because WebGPU does not support raytracing.
    struct GPUTlasBuildEntry
    {
        GPUTlasHandle    tlas;
        GPUTlasInstances instances;
    };

    // NOTE: Non-WebGPU standard because WebGPU does not support raytracing.
    struct GPUBlendComponent
    {
        GPUBlendOperation operation  = GPUBlendOperation::ADD;
        GPUBlendFactor    src_factor = GPUBlendFactor::ONE;
        GPUBlendFactor    dst_factor = GPUBlendFactor::ZERO;
    };

    struct GPUBlendState
    {
        GPUBlendComponent color = {};
        GPUBlendComponent alpha = {};
    };

    // NOTE: shader_semantic is non-WebGPU standard, and only applicable to D3D12.
    // Users are expected to use the reflection or serialization/deserialization API to automatically populate these.
    struct GPUVertexAttribute
    {
        GPUVertexFormat format;
        GPUSize64       offset;
        GPUIndex32      shader_location;
        CString         shader_semantic = nullptr;
    };

    struct GPUVertexBufferLayout
    {
        GPUSize64           array_stride;
        GPUVertexStepMode   step_mode = GPUVertexStepMode::VERTEX;
        GPUVertexAttributes attributes;
    };

    struct GPUStencilFaceState
    {
        GPUCompareFunction  compare       = GPUCompareFunction::ALWAYS;
        GPUStencilOperation fail_op       = GPUStencilOperation::KEEP;
        GPUStencilOperation depth_fail_op = GPUStencilOperation::KEEP;
        GPUStencilOperation pass_op       = GPUStencilOperation::KEEP;
    };

    struct GPUColorTargetState
    {
        GPUTextureFormat   format;
        GPUBlendState      blend;
        GPUColorWriteFlags write_mask   = GPUColorWrite::ALL;
        bool               blend_enable = false;
    };

    struct GPUPipelineConstant
    {
        CString                  key;
        GPUPipelineConstantValue value = 0;
    };

    struct GPUProgrammableStage
    {
        GPUShaderModuleHandle module;
        CString               entry_point = "main";
        GPUPipelineConstants  constants   = {};
    };

    struct GPUBufferBinding
    {
        GPUBufferHandle buffer;
        GPUSize64       offset = 0;
        GPUSize64       size   = 0;
    };

    // NOTE: index is non-WebGPU standard, because WebGPU does not support binding an array of resources.
    // When index = 0, it is the same as standard WebGPU's binding model.
    // When index > 0, this indicate binding to the specific index of the array of resources.
    // https://github.com/gpuweb/gpuweb/issues/822
    struct GPUBindGroupEntry
    {
        GPUIndex32      binding = 0;
        GPUIndex32      index   = 0;
        GPUResourceType type    = GPUResourceType::BUFFER;
        union
        {
            GPUBufferBinding     buffer = {};
            GPUSamplerHandle     sampler;
            GPUTextureViewHandle texture;
            GPUTlasHandle        tlas;
        };

        // default trivial constructor / destructor
        GPUBindGroupEntry() {}
        ~GPUBindGroupEntry() {}
    };

    // NOTE: count is non-WebGPU standard, because WebGPU does not support binding an array of resources.
    // When count = 1, we treat this the same way WebGPU handles binding.
    // When count > 0, we treat this as an array of resources.
    // When count = ~usize(0), we treat this bind group layout to be bindless.
    // https://github.com/gpuweb/gpuweb/issues/822
    struct GPUBindGroupLayoutEntry
    {
        GPUResourceType     type       = GPUResourceType::BUFFER;
        GPUBindingIndex     binding    = {};
        GPUShaderStageFlags visibility = GPUShaderStage(0);
        GPUIndex32          count      = 1;
        union
        {
            GPUBufferBindingLayout         buffer = {};
            GPUSamplerBindingLayout        sampler;
            GPUTextureBindingLayout        texture;
            GPUStorageTextureBindingLayout storage_texture;
            GPUBVHBindingLayout            bvh;
        };

        // default trivial constructor / destructor
        GPUBindGroupLayoutEntry() {}
        ~GPUBindGroupLayoutEntry() {}
    };

    struct GPUVertexState : public GPUProgrammableStage
    {
        GPUVertexBufferLayouts buffers;
    };

    struct GPUFragmentState : public GPUProgrammableStage
    {
        GPUColorTargetStates targets;
    };

    struct GPUPrimitiveState
    {
        GPUPrimitiveTopology topology           = GPUPrimitiveTopology::TRIANGLE_LIST;
        GPUIndexFormat       strip_index_format = GPUIndexFormat::UINT32;
        GPUFrontFace         front_face         = GPUFrontFace::CCW;
        GPUCullMode          cull_mode          = GPUCullMode::NONE;

        // Requires "depth-clip-control" feature.
        bool unclipped_depth = false;
    };

    struct GPUDepthStencilState
    {
        GPUTextureFormat    format;
        bool                depth_write_enabled;
        GPUCompareFunction  depth_compare          = GPUCompareFunction::ALWAYS;
        GPUStencilFaceState stencil_front          = {};
        GPUStencilFaceState stencil_back           = {};
        GPUStencilValue     stencil_read_mask      = 0xFFFFFFFF;
        GPUStencilValue     stencil_write_mask     = 0xFFFFFFFF;
        GPUDepthBias        depth_bias             = 0;
        float               depth_bias_constant    = 0;
        float               depth_bias_slope_scale = 0;
        float               depth_bias_clamp       = 0;
    };

    struct GPUMultisampleState
    {
        GPUSize32     count                     = 1;
        GPUSampleMask mask                      = 0xFFFFFFFF;
        bool          alpha_to_one_enabled      = false;
        bool          alpha_to_coverage_enabled = false;
    };

    struct GPURenderPassColorAttachment
    {
        GPUTextureViewHandle view;
        GPUIntegerCoordinate depth_slice;
        GPUTextureViewHandle resolve_target;
        GPUColor             clear_value;
        GPULoadOp            load_op;
        GPUStoreOp           store_op;
    };

    struct GPURenderPassDepthStencilAttachment
    {
        GPUTextureViewHandle view;
        float                depth_clear_value;
        GPULoadOp            depth_load_op;
        GPUStoreOp           depth_store_op;
        bool                 depth_read_only     = false;
        GPUStencilValue      stencil_clear_value = 0;
        GPULoadOp            stencil_load_op;
        GPUStoreOp           stencil_store_op;
        bool                 stencil_read_only = false;
    };

    struct GPUTextureSubresourceRange
    {
        GPUSize32 base_mip_level   = 0;
        GPUSize32 mip_level_count  = 1;
        GPUSize32 base_array_layer = 0;
        GPUSize32 array_layers     = 1;
    };

    // NOTE: Non-WebGPU standard API because WebGPU chooses to handle barriers implicitly.
    // https://github.com/gpuweb/gpuweb/issues/27
    struct GPUMemoryBarrier
    {
        GPUBarrierSync   src_sync;
        GPUBarrierSync   dst_sync;
        GPUBarrierAccess src_access;
        GPUBarrierAccess dst_access;
    };

    // NOTE: Non-WebGPU standard API because WebGPU chooses to handle barriers implicitly.
    // https://github.com/gpuweb/gpuweb/issues/27
    struct GPUBufferBarrier
    {
        GPUBarrierSync   src_sync;
        GPUBarrierSync   dst_sync;
        GPUBarrierAccess src_access;
        GPUBarrierAccess dst_access;
        GPUBufferHandle  buffer;
        GPUSize64        offset;
        GPUSize64        size;
    };

    // NOTE: Non-WebGPU standard API because WebGPU chooses to handle barriers implicitly.
    // https://github.com/gpuweb/gpuweb/issues/27
    struct GPUTextureBarrier
    {
        GPUBarrierSync             src_sync;
        GPUBarrierSync             dst_sync;
        GPUBarrierAccess           src_access;
        GPUBarrierAccess           dst_access;
        GPUBarrierLayout           src_layout;
        GPUBarrierLayout           dst_layout;
        GPUTextureHandle           texture;
        GPUTextureSubresourceRange subresources;
    };

} // namespace lyra

#endif // LYRA_ENGINE_GRAPHICS_RHIUTILS_H
