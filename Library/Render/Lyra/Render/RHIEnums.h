#pragma once

#ifndef LYRA_LIBRARY_PLUGIN_RHI_ENUMS_H
#define LYRA_LIBRARY_PLUGIN_RHI_ENUMS_H

#include <Lyra/Common/String.h>
#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/Compatibility.h>

namespace lyra
{
    enum struct RHIFlag : uint
    {
        DEBUG      = 0x1,
        VALIDATION = 0x2,
    };

    enum struct RHIBackend : uint
    {
        METAL,
        D3D12,
        VULKAN,
    };

    enum struct GPUObjectType : uint
    {
        SURFACE,
        FENCE,
        BUFFER,
        SAMPLER,
        TEXTURE,
        TEXTURE_VIEW,
        SHADER_MODULE,
        COMMAND_ENCODER,
        BIND_GROUP,
        BIND_GROUP_HEAP,
        BIND_GROUP_LAYOUT,
        PIPELINE_LAYOUT,
        RENDER_PIPELINE,
        COMPUTE_PIPELINE,
        RAYTRACING_PIPELINE,
        QUERY_SET,
        TLAS,
        BLAS,
    };

    enum struct GPUErrorFilter
    {
        VALIDATION,
        OUT_OF_MEMORY,
        INTERNAL,
    };

    enum struct GPUFeatureName : uint
    {
        CORE_FEATURES_AND_LIMITS,
        DEPTH_CLIP_CONTROL,
        DEPTH32FLOAT_STENCIL8,
        TEXTURE_COMPRESSION_BC,
        TEXTURE_COMPRESSION_BC_SLICED_3D,
        TEXTURE_COMPRESSION_ETC2,
        TEXTURE_COMPRESSION_ASTC,
        TEXTURE_COMPRESSION_ASTC_SLICED_3D,
        TIMESTAMP_QUERY,
        INDIRECT_FIRST_INSTANCE,
        SHADER_F16,
        RG11B10UFLOAT_PLUGINABLE,
        BGRA8UNORM_STORAGE,
        FLOAT32_FILTERABLE,
        FLOAT32_BLENDABLE,
        CLIP_DISTANCES,
        DUAL_SOURCE_BLENDING,
        SUBGROUPS,
        TEXTURE_FORMATS_TIER1,
        BINDLESS,
        RAYTRACING,
    };

    enum struct GPUQueueType : uint
    {
        DEFAULT,
        COMPUTE,
        TRANSFER,
    };

    enum struct GPUPresentMode : uint
    {
        Fifo,
        FifoRelaxed,
        Immediate,
        Mailbox,
    };

    enum struct GPUCompositeAlphaMode : uint
    {
        Opaque,
        PreMultiplied,
        PostMultiplied,
    };

    enum struct GPUColorSpace : uint
    {
        SRGB,
        DISPLAY_P3,
    };

    enum struct GPUColorWrite : uint
    {
        NONE  = 0x0,
        RED   = 0x1,
        GREEN = 0x2,
        BLUE  = 0x4,
        ALPHA = 0x8,
        ALL   = 0xF,
    };

    enum struct GPUBlendOperation : uint
    {
        ADD,
        SUBTRACT,
        REVERSE_SUBTRACT,
        MIN,
        MAX,
    };

    enum struct GPUBlendFactor : uint
    {
        ZERO,
        ONE,
        SRC,
        ONE_MINUS_SRC,
        SRC_ALPHA,
        ONE_MINUS_SRC_ALPHA,
        DST,
        ONE_MINUS_DST,
        DST_ALPHA,
        ONE_MINUS_DST_ALPHA,
        SRC_ALPHA_SATURATED,
        CONSTANT,
        ONE_MINUS_CONSTANT,
        SRC1,
        ONE_MINUS_SRC1,
        SRC1_ALPHA,
        ONE_MINUS_SRC1_ALPHA,
    };

    enum struct GPULoadOp : uint
    {
        LOAD,
        CLEAR,
    };

    enum struct GPUStoreOp : uint
    {
        STORE,
        DISCARD,
    };

    enum struct GPUMapMode : uint
    {
        READ  = 0x1,
        WRITE = 0x2,
    };

    enum struct GPUMapState : uint
    {
        UNMAPPED,
        PENDING,
        MAPPED,
    };

    enum struct GPUBufferUsage : uint
    {
        MAP_READ      = 0x0001,
        MAP_WRITE     = 0x0002,
        COPY_SRC      = 0x0004,
        COPY_DST      = 0x0008,
        INDEX         = 0x0010,
        VERTEX        = 0x0020,
        UNIFORM       = 0x0040,
        STORAGE       = 0x0080,
        INDIRECT      = 0x0100,
        QUERY_RESOLVE = 0x0200,
        BLAS_INPUT    = 0x0400,
        TLAS_INPUT    = 0x0800,
    };

    enum struct GPUTextureUsage : uint
    {
        COPY_SRC          = 0x01,
        COPY_DST          = 0x02,
        TEXTURE_BINDING   = 0x04,
        STORAGE_BINDING   = 0x08,
        RENDER_ATTACHMENT = 0x10,
    };

    enum struct GPUShaderStage : uint
    {
        NONE      = 0x0,
        VERTEX    = 0x1,
        FRAGMENT  = 0x2,
        COMPUTE   = 0x4,
        RAYGEN    = 0x8,
        MISS      = 0x10,
        CHIT      = 0x20,
        AHIT      = 0x40,
        INTERSECT = 0x80,
    };

    enum struct GPUResourceType : uint
    {
        BUFFER,
        SAMPLER,
        TEXTURE,
        STORAGE_TEXTURE,
        ACCELERATION_STRUCTURE,
    };

    enum struct GPUQueryType : uint
    {
        OCCLUSION,
        TIMESTAMP,
        BLAS_PROPERTIES,     // additional accelerations structure support
        PIPELINE_STATISTICS, // additional pipeline support
    };

    enum struct GPUBufferBindingType : uint
    {
        UNIFORM,
        STORAGE,
        READ_ONLY_STORAGE,
    };

    enum struct GPUSamplerBindingType : uint
    {
        FILTERING,
        NON_FILTERING,
        COMPARISON,
    };

    enum struct GPUTextureSampleType : uint
    {
        FLOAT,
        UNFILTERABLE_FLOAT,
        DEPTH,
        SINT,
        UINT,
    };

    enum struct GPUStorageTextureAccess : uint
    {
        WRITE_ONLY,
        READ_ONLY,
        READ_WRITE,
    };

    enum struct GPUTextureAspect : uint
    {
        ALL     = 0xFFFFFFFFu,
        COLOR   = 0x1u,
        DEPTH   = 0x2u,
        STENCIL = 0x4u,
    };

    enum struct GPUTextureDimension : uint
    {
        x1D,
        x2D,
        x3D,
    };

    enum struct GPUTextureViewDimension : uint
    {
        x1D,
        x2D,
        x2D_ARRAY,
        CUBE,
        CUBE_ARRAY,
        x3D,
    };

    enum struct GPUAddressMode : uint
    {
        CLAMP_TO_EDGE,
        REPEAT,
        MIRROR_REPEAT,
    };

    enum struct GPUFilterMode : uint
    {
        NEAREST,
        LINEAR,
    };

    enum struct GPUMipmapFilterMode : uint
    {
        NEAREST,
        LINEAR,
    };

    enum struct GPUCompareFunction : uint
    {
        NEVER,
        LESS,
        EQUAL,
        LESS_EQUAL,
        GREATER,
        NOT_EQUAL,
        GREATER_EQUAL,
        ALWAYS,
    };

    enum GPUStencilOperation : uint
    {
        KEEP,
        ZERO,
        REPLACE,
        INVERT,
        INCREMENT_CLAMP,
        DECREMENT_CLAMP,
        INCREMENT_WRAP,
        DECREMENT_WRAP,
    };

    enum struct GPUFrontFace : uint
    {
        CCW,
        CW,
    };

    enum struct GPUCullMode : uint
    {
        NONE,
        FRONT,
        BACK,
    };

    enum struct GPUPrimitiveTopology : uint
    {
        POINT_LIST,
        LINE_LIST,
        LINE_STRIP,
        TRIANGLE_LIST,
        TRIANGLE_STRIP,
    };

    enum struct GPUIndexFormat : uint
    {
        UINT16,
        UINT32,
    };

    enum struct GPUVertexStepMode : uint
    {
        VERTEX,
        INSTANCE,
    };

    enum struct GPUVertexFormat : uint
    {
        UINT8,
        UINT8x2,
        UINT8x4,
        SINT8,
        SINT8x2,
        SINT8x4,
        UNORM8,
        UNORM8x2,
        UNORM8x4,
        SNORM8,
        SNORM8x2,
        SNORM8x4,
        UINT16,
        UINT16x2,
        UINT16x4,
        SINT16,
        SINT16x2,
        SINT16x4,
        UNORM16,
        UNORM16x2,
        UNORM16x4,
        SNORM16,
        SNORM16x2,
        SNORM16x4,
        FLOAT16,
        FLOAT16x2,
        FLOAT16x4,
        FLOAT32,
        FLOAT32x2,
        FLOAT32x3,
        FLOAT32x4,
        UINT32,
        UINT32x2,
        UINT32x3,
        UINT32x4,
        SINT32,
        SINT32x2,
        SINT32x3,
        SINT32x4,
        UNORM10_10_10_2,
        UNORM8x4_BGRA,
    };

    enum struct GPUTextureFormat : uint
    {
        // 8-bit formats
        R8UNORM,
        R8SNORM,
        R8UINT,
        R8SINT,

        // 16-bit formats
        R16UNORM,
        R16SNORM,
        R16UINT,
        R16SINT,
        RG8UNORM,
        RG8SNORM,
        RG8UINT,
        RG8SINT,

        // 32-bit formats
        R32UINT,
        R32SINT,
        R32FLOAT,
        RG16UNORM,
        RG16SNORM,
        RG16UINT,
        RG16SINT,
        RG16FLOAT,
        RGBA8UNORM,
        RGBA8UNORM_SRGB,
        RGBA8SNORM,
        RGBA8UINT,
        RGBA8SINT,
        BGRA8UNORM,
        BGRA8UNORM_SRGB,

        // packed 32-bit formats
        RGB9E5UFLOAT,
        RGB10A2UINT,
        RGB10A2UNORM,
        RG11B10UFLOAT,

        // 64-bit formats
        RG32UINT,
        RG32SINT,
        RG32FLOAT,
        RGBA16UNORM,
        RGBA16SNORM,
        RGBA16UINT,
        RGBA16SINT,
        RGBA16FLOAT,

        // 128-bit formats
        RGBA32UINT,
        RGBA32SINT,
        RGBA32FLOAT,

        // depth/stencil formats
        STENCIL8,
        DEPTH16UNORM,
        DEPTH24PLUS,
        DEPTH24PLUS_STENCIL8,
        DEPTH32FLOAT,

        // depth32float-stencil8 feature
        DEPTH32FLOAT_STENCIL8,

        // BC compressed formats usable if texture-compression-bc is both
        // supported by the device/user agent and enabled in requestDevice.
        BC1_RGBA_UNORM,
        BC1_RGBA_UNORM_SRGB,
        BC2_RGBA_UNORM,
        BC2_RGBA_UNORM_SRGB,
        BC3_RGBA_UNORM,
        BC3_RGBA_UNORM_SRGB,
        BC4_R_UNORM,
        BC4_R_SNORM,
        BC5_RG_UNORM,
        BC5_RG_SNORM,
        BC6H_RGB_UFLOAT,
        BC6H_RGB_FLOAT,
        BC7_RGBA_UNORM,
        BC7_RGBA_UNORM_SRGB,

        // ETC2 compressed formats usable if texture_compression_etc2 is both
        // supported by the device/user agent and enabled in requestDevice.
        ETC2_RGB8UNORM,
        ETC2_RGB8UNORM_SRGB,
        ETC2_RGB8A1UNORM,
        ETC2_RGB8A1UNORM_SRGB,
        ETC2_RGBA8UNORM,
        ETC2_RGBA8UNORM_SRGB,
        EAC_R11UNORM,
        EAC_R11SNORM,
        EAC_RG11UNORM,
        EAC_RG11SNORM,

        // ASTC compressed formats usable if texture_compression_astc is both
        // supported by the device/user agent and enabled in requestDevice.
        ASTC_4X4_UNORM,
        ASTC_4X4_UNORM_SRGB,
        ASTC_5X4_UNORM,
        ASTC_5X4_UNORM_SRGB,
        ASTC_5X5_UNORM,
        ASTC_5X5_UNORM_SRGB,
        ASTC_6X5_UNORM,
        ASTC_6X5_UNORM_SRGB,
        ASTC_6X6_UNORM,
        ASTC_6X6_UNORM_SRGB,
        ASTC_8X5_UNORM,
        ASTC_8X5_UNORM_SRGB,
        ASTC_8X6_UNORM,
        ASTC_8X6_UNORM_SRGB,
        ASTC_8X8_UNORM,
        ASTC_8X8_UNORM_SRGB,
        ASTC_10X5_UNORM,
        ASTC_10X5_UNORM_SRGB,
        ASTC_10X6_UNORM,
        ASTC_10X6_UNORM_SRGB,
        ASTC_10X8_UNORM,
        ASTC_10X8_UNORM_SRGB,
        ASTC_10X10_UNORM,
        ASTC_10X10_UNORM_SRGB,
        ASTC_12X10_UNORM,
        ASTC_12X10_UNORM_SRGB,
        ASTC_12X12_UNORM,
        ASTC_12X12_UNORM_SRGB,
    };

    enum struct GPUBarrierSync : uint
    {
        NONE                         = 0x1u << 0,
        ALL                          = 0x1u << 1,
        DRAW                         = 0x1u << 2,
        INDEX_INPUT                  = 0x1u << 4,
        VERTEX_SHADING               = 0x1u << 5,
        PIXEL_SHADING                = 0x1u << 6,
        DEPTH_STENCIL                = 0x1u << 7,
        RENDER_TARGET                = 0x1u << 8,
        COMPUTE                      = 0x1u << 9,
        RAYTRACING                   = 0x1u << 10,
        COPY                         = 0x1u << 11,
        BLIT                         = 0x1u << 12,
        CLEAR                        = 0x1u << 13,
        RESOLVE                      = 0x1u << 14,
        EXECUTE_INDIRECT             = 0x1u << 15,
        ALL_SHADING                  = 0x1u << 16,
        VIDEO_DECODE                 = 0x1u << 17,
        VIDEO_ENCODE                 = 0x1u << 18,
        ACCELERATION_STRUCTURE_BUILD = 0x1u << 19,
        ACCELERATION_STRUCTURE_COPY  = 0x1u << 20,
    };

    enum struct GPUBarrierAccess : uint
    {
        COMMON                       = 0x1u << 0,
        VERTEX_BUFFER                = 0x1u << 1,
        UNIFORM_BUFFER               = 0x1u << 2,
        INDEX_BUFFER                 = 0x1u << 4,
        RENDER_TARGET                = 0x1u << 5,
        UNORDERED_ACCESS             = 0x1u << 6,
        DEPTH_STENCIL_WRITE          = 0x1u << 7,
        DEPTH_STENCIL_READ           = 0x1u << 8,
        SHADER_RESOURCE              = 0x1u << 9,
        STREAM_OUTPUT                = 0x1u << 10,
        INDIRECT_ARGUMENT            = 0x1u << 11,
        COPY_DEST                    = 0x1u << 12,
        COPY_SOURCE                  = 0x1u << 13,
        RESOLVE_DEST                 = 0x1u << 14,
        RESOLVE_SOURCE               = 0x1u << 15,
        ACCELERATION_STRUCTURE_READ  = 0x1u << 16,
        ACCELERATION_STRUCTURE_WRITE = 0x1u << 17,
        SHADING_RATE_SOURCE          = 0x1u << 18,
        VIDEO_DECODE_READ            = 0x1u << 19,
        VIDEO_DECODE_WRITE           = 0x1u << 20,
        VIDEO_PROCESS_READ           = 0x1u << 21,
        VIDEO_PROCESS_WRITE          = 0x1u << 22,
        VIDEO_ENCODE_READ            = 0x1u << 23,
        VIDEO_ENCODE_WRITE           = 0x1u << 24,
        NO_ACCESS                    = 0x1u << 25,
    };

    enum struct GPUBarrierLayout : uint
    {
        UNDEFINED,
        COMMON,
        PRESENT,
        GENERIC_READ,
        RENDER_TARGET,
        UNORDERED_ACCESS,
        DEPTH_STENCIL_WRITE,
        DEPTH_STENCIL_READ,
        SHADER_RESOURCE,
        COPY_SOURCE,
        COPY_DEST,
        RESOLVE_SOURCE,
        RESOLVE_DEST,
        SHADING_RATE_SOURCE,
        VIDEO_DECODE_READ,
        VIDEO_DECODE_WRITE,
        VIDEO_PROCESS_READ,
        VIDEO_PROCESS_WRITE,
        VIDEO_ENCODE_READ,
        VIDEO_ENCODE_WRITE,
        DIRECT_QUEUE_COMMON,
        DIRECT_QUEUE_GENERIC_READ,
        DIRECT_QUEUE_UNORDERED_ACCESS,
        DIRECT_QUEUE_SHADER_RESOURCE,
        DIRECT_QUEUE_COPY_SOURCE,
        DIRECT_QUEUE_COPY_DEST,
        COMPUTE_QUEUE_COMMON,
        COMPUTE_QUEUE_GENERIC_READ,
        COMPUTE_QUEUE_UNORDERED_ACCESS,
        COMPUTE_QUEUE_SHADER_RESOURCE,
        COMPUTE_QUEUE_COPY_SOURCE,
        COMPUTE_QUEUE_COPY_DEST,
        VIDEO_QUEUE_COMMON
    };

    enum struct GPUBlasType : uint
    {
        TRIANGLE,
    };

    enum struct GPUBVHFlag : uint
    {
        ALLOW_UPDATE                = 0x0001,
        ALLOW_COMPACTION            = 0x0002,
        PREFER_FAST_TRACE           = 0x0004,
        PREFER_FAST_BUILD           = 0x0008,
        LOW_MEMORY                  = 0x0010,
        USE_TRANSFORM               = 0x0020,
        ALLOW_RAY_HIT_VERTEX_RETURN = 0x0040,
    };

    enum struct GPUBVHUpdateMode : uint
    {
        BUILD,
        PREFER_UPDATE,
    };

    enum struct GPUBVHGeometryFlag : uint
    {
        BVH_OPAQUE                      = 0x0001,
        NO_DUPLICATE_ANY_HIT_INVOCATION = 0x0002,
    };

    inline bool is_depth_format(GPUTextureFormat format)
    {
        switch (format) {
            case GPUTextureFormat::DEPTH16UNORM:
            case GPUTextureFormat::DEPTH24PLUS:
            case GPUTextureFormat::DEPTH24PLUS_STENCIL8:
            case GPUTextureFormat::DEPTH32FLOAT:
            case GPUTextureFormat::DEPTH32FLOAT_STENCIL8:
                return true;
            default:
                return false;
        }
    }

    inline bool is_stencil_format(GPUTextureFormat format)
    {
        switch (format) {
            case GPUTextureFormat::STENCIL8:
            case GPUTextureFormat::DEPTH24PLUS_STENCIL8:
            case GPUTextureFormat::DEPTH32FLOAT_STENCIL8:
                return true;
            default:
                return false;
        }
    }

    inline bool is_depth_stencil_format(GPUTextureFormat format)
    {
        return is_depth_format(format) || is_stencil_format(format);
    }

    inline constexpr CString to_string(GPUObjectType type)
    {
        // clang-format off
        switch (type) {
            case GPUObjectType::FENCE:               return "GPUFence";
            case GPUObjectType::BUFFER:              return "GPUBuffer";
            case GPUObjectType::SAMPLER:             return "GPUSampler";
            case GPUObjectType::TEXTURE:             return "GPUTexture";
            case GPUObjectType::TEXTURE_VIEW:        return "GPUTextureView";
            case GPUObjectType::SHADER_MODULE:       return "GPUShaderModule";
            case GPUObjectType::COMMAND_ENCODER:     return "GPUCommandEncoder";
            case GPUObjectType::BIND_GROUP:          return "GPUBindGroup";
            case GPUObjectType::BIND_GROUP_LAYOUT:   return "GPUBindGroupLayout";
            case GPUObjectType::PIPELINE_LAYOUT:     return "GPUPipelineLayout";
            case GPUObjectType::RENDER_PIPELINE:     return "GPURenderPipeline";
            case GPUObjectType::COMPUTE_PIPELINE:    return "GPUComputePipeline";
            case GPUObjectType::RAYTRACING_PIPELINE: return "GPURayTracingPipeline";
            case GPUObjectType::QUERY_SET:           return "GPUQuerySet";
            case GPUObjectType::TLAS:                return "GPUTlas";
            case GPUObjectType::BLAS:                return "GPUBlas";
            default:                                 return "Unknown";
        }
        // clang-format on
    }

} // namespace lyra

#endif // LYRA_LIBRARY_PLUGIN_RHI_ENUMS_H
