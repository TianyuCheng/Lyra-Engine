#include "D3D12Utils.h"
#include "d3d12.h"
#include "dxgiformat.h"

static Logger logger = create_logger("D3D12", LogLevel::trace);

static D3D12RHI* D3D12_RHI = nullptr;

void set_rhi(D3D12RHI* instance)
{
    D3D12_RHI = instance;
}

auto get_rhi() -> D3D12RHI*
{
    return D3D12_RHI;
}

Logger get_logger()
{
    return logger;
}

D3D12PresentMode infer_present_mode(GPUPresentMode mode)
{
    switch (mode) {
        case GPUPresentMode::Immediate:
            return D3D12PresentMode{0, DXGI_PRESENT_ALLOW_TEARING};
        case GPUPresentMode::Mailbox:
            return D3D12PresentMode{1, DXGI_PRESENT_DO_NOT_WAIT};
        case GPUPresentMode::Fifo:
        case GPUPresentMode::FifoRelaxed: // not supported, fallback to regular vsync
        default:
            return D3D12PresentMode{1, 0};
    }
}

D3D12_HEAP_TYPE infer_heap_type(GPUBufferUsageFlags usages)
{
    D3D12_HEAP_TYPE type = D3D12_HEAP_TYPE_DEFAULT;

    if (usages.contains(GPUBufferUsage::MAP_READ))
        return D3D12_HEAP_TYPE_READBACK;

    if (usages.contains(GPUBufferUsage::MAP_WRITE))
        return D3D12_HEAP_TYPE_UPLOAD;

    return type;
}

D3D12_RESOURCE_FLAGS infer_buffer_flags(GPUBufferUsageFlags usages)
{
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;

    if (usages.contains(GPUBufferUsage::STORAGE)) {
        flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    return flags;
}

D3D12_RESOURCE_FLAGS infer_texture_flags(GPUTextureUsageFlags usages, GPUTextureFormat format)
{
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;

    bool is_depth_stencil = is_depth_stencil_format(format);

    if (usages.contains(GPUTextureUsage::RENDER_ATTACHMENT)) {
        if (is_depth_stencil) {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        } else {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        }
    }

    if (usages.contains(GPUTextureUsage::STORAGE_BINDING)) {
        flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    // DENY SHADER RESOURCE is allowed only when some of following flags are set
    // D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
    // D3D12_RESOURCE_FLAG_VIDEO_DECODE_REFERENCE_ONLY
    // D3D12_RESOURCE_FLAG_VIDEO_ENCODE_REFERENCE_ONLY
    if (!usages.contains(GPUTextureUsage::TEXTURE_BINDING)) {
        if (flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
            flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
        if (flags & D3D12_RESOURCE_FLAG_VIDEO_DECODE_REFERENCE_ONLY)
            flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
        if (flags & D3D12_RESOURCE_FLAG_VIDEO_ENCODE_REFERENCE_ONLY)
            flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
    }

    return flags;
}

DXGI_FORMAT infer_texture_format(GPUTextureFormat format)
{
    switch (format) {
        case GPUTextureFormat::R8UNORM:
            return DXGI_FORMAT_R8_UNORM;
        case GPUTextureFormat::R8SNORM:
            return DXGI_FORMAT_R8_SNORM;
        case GPUTextureFormat::R8UINT:
            return DXGI_FORMAT_R8_UINT;
        case GPUTextureFormat::R8SINT:
            return DXGI_FORMAT_R8_SINT;
        case GPUTextureFormat::R16UNORM:
            return DXGI_FORMAT_R16_UNORM;
        case GPUTextureFormat::R16SNORM:
            return DXGI_FORMAT_R16_SNORM;
        case GPUTextureFormat::R16UINT:
            return DXGI_FORMAT_R16_UINT;
        case GPUTextureFormat::R16SINT:
            return DXGI_FORMAT_R16_SINT;
        case GPUTextureFormat::RG8UNORM:
            return DXGI_FORMAT_R8G8_UNORM;
        case GPUTextureFormat::RG8SNORM:
            return DXGI_FORMAT_R8G8_SNORM;
        case GPUTextureFormat::RG8UINT:
            return DXGI_FORMAT_R8G8_UINT;
        case GPUTextureFormat::RG8SINT:
            return DXGI_FORMAT_R8G8_SINT;
        case GPUTextureFormat::R32UINT:
            return DXGI_FORMAT_R32_UINT;
        case GPUTextureFormat::R32SINT:
            return DXGI_FORMAT_R32_SINT;
        case GPUTextureFormat::R32FLOAT:
            return DXGI_FORMAT_R32_FLOAT;
        case GPUTextureFormat::RG16UNORM:
            return DXGI_FORMAT_R16G16_UNORM;
        case GPUTextureFormat::RG16SNORM:
            return DXGI_FORMAT_R16G16_SNORM;
        case GPUTextureFormat::RG16UINT:
            return DXGI_FORMAT_R16G16_UINT;
        case GPUTextureFormat::RG16SINT:
            return DXGI_FORMAT_R16G16_SINT;
        case GPUTextureFormat::RG16FLOAT:
            return DXGI_FORMAT_R16G16_FLOAT;
        case GPUTextureFormat::RGBA8UNORM:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case GPUTextureFormat::RGBA8UNORM_SRGB:
            return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case GPUTextureFormat::RGBA8SNORM:
            return DXGI_FORMAT_R8G8B8A8_SNORM;
        case GPUTextureFormat::RGBA8UINT:
            return DXGI_FORMAT_R8G8B8A8_UINT;
        case GPUTextureFormat::RGBA8SINT:
            return DXGI_FORMAT_R8G8B8A8_SINT;
        case GPUTextureFormat::BGRA8UNORM:
            return DXGI_FORMAT_B8G8R8A8_UNORM;
        case GPUTextureFormat::BGRA8UNORM_SRGB:
            return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        case GPUTextureFormat::RGB9E5UFLOAT:
            return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
        case GPUTextureFormat::RGB10A2UINT:
            return DXGI_FORMAT_R10G10B10A2_UINT;
        case GPUTextureFormat::RGB10A2UNORM:
            return DXGI_FORMAT_R10G10B10A2_UNORM;
        case GPUTextureFormat::RG11B10UFLOAT: // TODO: Check if this mapping is correct.
            return DXGI_FORMAT_R10G10B10A2_TYPELESS;
        case GPUTextureFormat::RG32UINT:
            return DXGI_FORMAT_R32G32_UINT;
        case GPUTextureFormat::RG32SINT:
            return DXGI_FORMAT_R32G32_SINT;
        case GPUTextureFormat::RG32FLOAT:
            return DXGI_FORMAT_R32G32_FLOAT;
        case GPUTextureFormat::RGBA16UNORM:
            return DXGI_FORMAT_R16G16B16A16_UNORM;
        case GPUTextureFormat::RGBA16SNORM:
            return DXGI_FORMAT_R16G16B16A16_SNORM;
        case GPUTextureFormat::RGBA16UINT:
            return DXGI_FORMAT_R16G16B16A16_UINT;
        case GPUTextureFormat::RGBA16SINT:
            return DXGI_FORMAT_R16G16B16A16_SINT;
        case GPUTextureFormat::RGBA16FLOAT:
            return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case GPUTextureFormat::RGBA32UINT:
            return DXGI_FORMAT_R32G32B32A32_UINT;
        case GPUTextureFormat::RGBA32SINT:
            return DXGI_FORMAT_R32G32B32A32_SINT;
        case GPUTextureFormat::RGBA32FLOAT:
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case GPUTextureFormat::STENCIL8: // TODO: Check if this mapping is correct.
            return DXGI_FORMAT_R24G8_TYPELESS;
        case GPUTextureFormat::DEPTH16UNORM:
            return DXGI_FORMAT_D16_UNORM;
        case GPUTextureFormat::DEPTH24PLUS:
            return DXGI_FORMAT_D24_UNORM_S8_UINT; // TODO: Check if this mapping is correct.
        case GPUTextureFormat::DEPTH24PLUS_STENCIL8:
            return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case GPUTextureFormat::DEPTH32FLOAT:
            return DXGI_FORMAT_D32_FLOAT;
        case GPUTextureFormat::DEPTH32FLOAT_STENCIL8:
            return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
        case GPUTextureFormat::BC1_RGBA_UNORM:
            return DXGI_FORMAT_BC1_UNORM;
        case GPUTextureFormat::BC1_RGBA_UNORM_SRGB:
            return DXGI_FORMAT_BC1_UNORM_SRGB;
        case GPUTextureFormat::BC2_RGBA_UNORM:
            return DXGI_FORMAT_BC2_UNORM;
        case GPUTextureFormat::BC2_RGBA_UNORM_SRGB:
            return DXGI_FORMAT_BC2_UNORM_SRGB;
        case GPUTextureFormat::BC3_RGBA_UNORM:
            return DXGI_FORMAT_BC3_UNORM;
        case GPUTextureFormat::BC3_RGBA_UNORM_SRGB:
            return DXGI_FORMAT_BC3_UNORM_SRGB;
        case GPUTextureFormat::BC4_R_UNORM:
            return DXGI_FORMAT_BC4_UNORM;
        case GPUTextureFormat::BC4_R_SNORM:
            return DXGI_FORMAT_BC4_SNORM;
        case GPUTextureFormat::BC5_RG_UNORM:
            return DXGI_FORMAT_BC5_UNORM;
        case GPUTextureFormat::BC5_RG_SNORM:
            return DXGI_FORMAT_BC5_SNORM;
        case GPUTextureFormat::BC6H_RGB_UFLOAT:
            return DXGI_FORMAT_BC6H_UF16;
        case GPUTextureFormat::BC6H_RGB_FLOAT:
            return DXGI_FORMAT_BC6H_SF16;
        case GPUTextureFormat::BC7_RGBA_UNORM:
            return DXGI_FORMAT_BC7_UNORM;
        case GPUTextureFormat::BC7_RGBA_UNORM_SRGB:
            return DXGI_FORMAT_BC7_UNORM_SRGB;
        case GPUTextureFormat::ETC2_RGB8UNORM:
        case GPUTextureFormat::ETC2_RGB8UNORM_SRGB:
        case GPUTextureFormat::ETC2_RGB8A1UNORM:
        case GPUTextureFormat::ETC2_RGB8A1UNORM_SRGB:
        case GPUTextureFormat::ETC2_RGBA8UNORM:
        case GPUTextureFormat::ETC2_RGBA8UNORM_SRGB:
        case GPUTextureFormat::EAC_R11UNORM:
        case GPUTextureFormat::EAC_R11SNORM:
        case GPUTextureFormat::EAC_RG11UNORM:
        case GPUTextureFormat::EAC_RG11SNORM:
            assert(!!!"D3D12 does not support EAC formats!");
            return DXGI_FORMAT_UNKNOWN;
        case GPUTextureFormat::ASTC_4X4_UNORM:
        case GPUTextureFormat::ASTC_4X4_UNORM_SRGB:
        case GPUTextureFormat::ASTC_5X4_UNORM:
        case GPUTextureFormat::ASTC_5X4_UNORM_SRGB:
        case GPUTextureFormat::ASTC_5X5_UNORM:
        case GPUTextureFormat::ASTC_5X5_UNORM_SRGB:
        case GPUTextureFormat::ASTC_6X5_UNORM:
        case GPUTextureFormat::ASTC_6X5_UNORM_SRGB:
        case GPUTextureFormat::ASTC_6X6_UNORM:
        case GPUTextureFormat::ASTC_6X6_UNORM_SRGB:
        case GPUTextureFormat::ASTC_8X5_UNORM:
        case GPUTextureFormat::ASTC_8X5_UNORM_SRGB:
        case GPUTextureFormat::ASTC_8X6_UNORM:
        case GPUTextureFormat::ASTC_8X6_UNORM_SRGB:
        case GPUTextureFormat::ASTC_8X8_UNORM:
        case GPUTextureFormat::ASTC_8X8_UNORM_SRGB:
        case GPUTextureFormat::ASTC_10X5_UNORM:
        case GPUTextureFormat::ASTC_10X5_UNORM_SRGB:
        case GPUTextureFormat::ASTC_10X6_UNORM:
        case GPUTextureFormat::ASTC_10X6_UNORM_SRGB:
        case GPUTextureFormat::ASTC_10X8_UNORM:
        case GPUTextureFormat::ASTC_10X8_UNORM_SRGB:
        case GPUTextureFormat::ASTC_10X10_UNORM:
        case GPUTextureFormat::ASTC_10X10_UNORM_SRGB:
        case GPUTextureFormat::ASTC_12X10_UNORM:
        case GPUTextureFormat::ASTC_12X10_UNORM_SRGB:
        case GPUTextureFormat::ASTC_12X12_UNORM:
        case GPUTextureFormat::ASTC_12X12_UNORM_SRGB:
            assert(!!!"D3D12 does not support ASTC formats!");
            return DXGI_FORMAT_UNKNOWN;
    }
    throw std::invalid_argument("invalid argument for GPUTextureFormat");
}

D3D12_PRIMITIVE_TOPOLOGY_TYPE infer_topology_type(GPUPrimitiveTopology topology)
{
    switch (topology) {
        case GPUPrimitiveTopology::POINT_LIST:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        case GPUPrimitiveTopology::LINE_LIST:
        case GPUPrimitiveTopology::LINE_STRIP:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        case GPUPrimitiveTopology::TRIANGLE_LIST:
        case GPUPrimitiveTopology::TRIANGLE_STRIP:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        default:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    }
}

D3D12_PRIMITIVE_TOPOLOGY infer_topology(GPUPrimitiveTopology topology)
{
    switch (topology) {
        case GPUPrimitiveTopology::POINT_LIST:
            return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
        case GPUPrimitiveTopology::LINE_LIST:
            return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        case GPUPrimitiveTopology::LINE_STRIP:
            return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case GPUPrimitiveTopology::TRIANGLE_LIST:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case GPUPrimitiveTopology::TRIANGLE_STRIP:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        default:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    }
}

uint infer_row_pitch(DXGI_FORMAT format, uint width, uint bytes_per_row)
{
    if (bytes_per_row != 0)
        return bytes_per_row;

    constexpr uint alignment = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT; // 256 bytes
    return (size_of(format) * width + alignment - 1) & ~(alignment - 1);
}

DXGI_ALPHA_MODE d3d12enum(GPUCompositeAlphaMode mode)
{
    switch (mode) {
        case GPUCompositeAlphaMode::Opaque:
            return DXGI_ALPHA_MODE_IGNORE;
        case GPUCompositeAlphaMode::PreMultiplied:
            return DXGI_ALPHA_MODE_PREMULTIPLIED;
        case GPUCompositeAlphaMode::PostMultiplied:
            return DXGI_ALPHA_MODE_STRAIGHT;
        default:
            // handle error case
            return DXGI_ALPHA_MODE_UNSPECIFIED;
    }
}
DXGI_SWAP_EFFECT d3d12enum(GPUPresentMode mode)
{
    switch (mode) {
        case GPUPresentMode::Fifo:
            return DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        case GPUPresentMode::FifoRelaxed:
            return DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        case GPUPresentMode::Immediate:
            return DXGI_SWAP_EFFECT_FLIP_DISCARD;
        case GPUPresentMode::Mailbox:
            return DXGI_SWAP_EFFECT_FLIP_DISCARD;
        default:
            // handle error case
            return DXGI_SWAP_EFFECT_FLIP_DISCARD;
    }
}

D3D12_SHADER_VISIBILITY d3d12enum(GPUShaderStageFlags stages)
{
    bool has_compute  = stages.contains(GPUShaderStage::COMPUTE);
    bool has_vertex   = stages.contains(GPUShaderStage::VERTEX);
    bool has_fragment = stages.contains(GPUShaderStage::FRAGMENT);

    D3D12_SHADER_VISIBILITY visibility;
    if (has_compute) {
        visibility = D3D12_SHADER_VISIBILITY_ALL; // Compute uses ALL
    } else if (has_vertex && !has_fragment && !has_compute) {
        visibility = D3D12_SHADER_VISIBILITY_VERTEX;
    } else if (has_fragment && !has_vertex && !has_compute) {
        visibility = D3D12_SHADER_VISIBILITY_PIXEL;
    } else {
        visibility = D3D12_SHADER_VISIBILITY_ALL; // Multiple stages or mixed usage
    }
    return visibility;
}

D3D12_COMPARISON_FUNC d3d12enum(GPUCompareFunction compare, bool enable)
{
    if (!enable)
        // (a windows update removed this enum), need to check if it will ever be back
        return D3D12_COMPARISON_FUNC(0); // D3D12_COMPARISON_FUNC_NONE;

    switch (compare) {
        case GPUCompareFunction::NEVER:
            return D3D12_COMPARISON_FUNC_NEVER;
        case GPUCompareFunction::LESS:
            return D3D12_COMPARISON_FUNC_LESS;
        case GPUCompareFunction::EQUAL:
            return D3D12_COMPARISON_FUNC_EQUAL;
        case GPUCompareFunction::LESS_EQUAL:
            return D3D12_COMPARISON_FUNC_LESS_EQUAL;
        case GPUCompareFunction::GREATER:
            return D3D12_COMPARISON_FUNC_GREATER;
        case GPUCompareFunction::NOT_EQUAL:
            return D3D12_COMPARISON_FUNC_NOT_EQUAL;
        case GPUCompareFunction::GREATER_EQUAL:
            return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        case GPUCompareFunction::ALWAYS:
            return D3D12_COMPARISON_FUNC_ALWAYS;
        default:
            return D3D12_COMPARISON_FUNC_ALWAYS;
    }
}

D3D12_FILTER d3d12enum(GPUFilterMode min, GPUFilterMode mag, GPUMipmapFilterMode mip)
{
    bool min_linear = (min == GPUFilterMode::LINEAR);
    bool mag_linear = (mag == GPUFilterMode::LINEAR);
    bool mip_linear = (mip == GPUMipmapFilterMode::LINEAR);

    if (!min_linear && !mag_linear && !mip_linear)
        return D3D12_FILTER_MIN_MAG_MIP_POINT;
    else if (!min_linear && !mag_linear && mip_linear)
        return D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
    else if (!min_linear && mag_linear && !mip_linear)
        return D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
    else if (!min_linear && mag_linear && mip_linear)
        return D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
    else if (min_linear && !mag_linear && !mip_linear)
        return D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
    else if (min_linear && !mag_linear && mip_linear)
        return D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
    else if (min_linear && mag_linear && !mip_linear)
        return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    else // min_linear && mag_linear && mip_linear
        return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
}

D3D12_TEXTURE_ADDRESS_MODE d3d12enum(GPUAddressMode mode)
{
    switch (mode) {
        case GPUAddressMode::REPEAT:
            return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        case GPUAddressMode::MIRROR_REPEAT:
            return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        case GPUAddressMode::CLAMP_TO_EDGE:
            return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    }
    throw std::invalid_argument("invalid argument for GPUAddressMode");
}

DXGI_FORMAT d3d12enum(GPUVertexFormat format)
{
    switch (format) {
        case GPUVertexFormat::UINT8:
            return DXGI_FORMAT_R8_UINT;
        case GPUVertexFormat::UINT8x2:
            return DXGI_FORMAT_R8G8_UINT;
        case GPUVertexFormat::UINT8x4:
            return DXGI_FORMAT_R8G8B8A8_UINT;
        case GPUVertexFormat::SINT8:
            return DXGI_FORMAT_R8_SINT;
        case GPUVertexFormat::SINT8x2:
            return DXGI_FORMAT_R8G8_SINT;
        case GPUVertexFormat::SINT8x4:
            return DXGI_FORMAT_R8G8B8A8_SINT;
        case GPUVertexFormat::UNORM8:
            return DXGI_FORMAT_R8_UNORM;
        case GPUVertexFormat::UNORM8x2:
            return DXGI_FORMAT_R8G8_UNORM;
        case GPUVertexFormat::UNORM8x4:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case GPUVertexFormat::SNORM8:
            return DXGI_FORMAT_R8_SNORM;
        case GPUVertexFormat::SNORM8x2:
            return DXGI_FORMAT_R8G8_SNORM;
        case GPUVertexFormat::SNORM8x4:
            return DXGI_FORMAT_R8G8B8A8_SNORM;
        case GPUVertexFormat::UINT16:
            return DXGI_FORMAT_R16_UINT;
        case GPUVertexFormat::UINT16x2:
            return DXGI_FORMAT_R16G16_UINT;
        case GPUVertexFormat::UINT16x4:
            return DXGI_FORMAT_R16G16B16A16_UINT;
        case GPUVertexFormat::SINT16:
            return DXGI_FORMAT_R16_SINT;
        case GPUVertexFormat::SINT16x2:
            return DXGI_FORMAT_R16G16_SINT;
        case GPUVertexFormat::SINT16x4:
            return DXGI_FORMAT_R16G16B16A16_SINT;
        case GPUVertexFormat::UNORM16:
            return DXGI_FORMAT_R16_UNORM;
        case GPUVertexFormat::UNORM16x2:
            return DXGI_FORMAT_R16G16_UNORM;
        case GPUVertexFormat::UNORM16x4:
            return DXGI_FORMAT_R16G16B16A16_UNORM;
        case GPUVertexFormat::SNORM16:
            return DXGI_FORMAT_R16_SNORM;
        case GPUVertexFormat::SNORM16x2:
            return DXGI_FORMAT_R16G16_SNORM;
        case GPUVertexFormat::SNORM16x4:
            return DXGI_FORMAT_R16G16B16A16_SNORM;
        case GPUVertexFormat::FLOAT16:
            return DXGI_FORMAT_R16_FLOAT;
        case GPUVertexFormat::FLOAT16x2:
            return DXGI_FORMAT_R16G16_FLOAT;
        case GPUVertexFormat::FLOAT16x4:
            return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case GPUVertexFormat::FLOAT32:
            return DXGI_FORMAT_R32_FLOAT;
        case GPUVertexFormat::FLOAT32x2:
            return DXGI_FORMAT_R32G32_FLOAT;
        case GPUVertexFormat::FLOAT32x3:
            return DXGI_FORMAT_R32G32B32_FLOAT;
        case GPUVertexFormat::FLOAT32x4:
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case GPUVertexFormat::UINT32:
            return DXGI_FORMAT_R32_UINT;
        case GPUVertexFormat::UINT32x2:
            return DXGI_FORMAT_R32G32_UINT;
        case GPUVertexFormat::UINT32x3:
            return DXGI_FORMAT_R32G32B32_UINT;
        case GPUVertexFormat::UINT32x4:
            return DXGI_FORMAT_R32G32B32A32_UINT;
        case GPUVertexFormat::SINT32:
            return DXGI_FORMAT_R32_SINT;
        case GPUVertexFormat::SINT32x2:
            return DXGI_FORMAT_R32G32_SINT;
        case GPUVertexFormat::SINT32x3:
            return DXGI_FORMAT_R32G32B32_SINT;
        case GPUVertexFormat::SINT32x4:
            return DXGI_FORMAT_R32G32B32A32_SINT;
        default:
            return DXGI_FORMAT_UNKNOWN;
    }
}

D3D12_INPUT_CLASSIFICATION d3d12enum(GPUVertexStepMode stepMode)
{
    switch (stepMode) {
        case GPUVertexStepMode::VERTEX:
            return D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        case GPUVertexStepMode::INSTANCE:
            return D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
        default:
            return D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    }
}

D3D12_CULL_MODE d3d12enum(GPUCullMode cull)
{
    switch (cull) {
        case GPUCullMode::NONE:
            return D3D12_CULL_MODE_NONE;
        case GPUCullMode::FRONT:
            return D3D12_CULL_MODE_FRONT;
        case GPUCullMode::BACK:
            return D3D12_CULL_MODE_BACK;
        default:
            return D3D12_CULL_MODE_BACK;
    }
}

D3D12_STENCIL_OP d3d12enum(GPUStencilOperation op)
{
    switch (op) {
        case GPUStencilOperation::KEEP:
            return D3D12_STENCIL_OP_KEEP;
        case GPUStencilOperation::ZERO:
            return D3D12_STENCIL_OP_ZERO;
        case GPUStencilOperation::REPLACE:
            return D3D12_STENCIL_OP_REPLACE;
        case GPUStencilOperation::INVERT:
            return D3D12_STENCIL_OP_INVERT;
        case GPUStencilOperation::INCREMENT_CLAMP:
            return D3D12_STENCIL_OP_INCR_SAT;
        case GPUStencilOperation::DECREMENT_CLAMP:
            return D3D12_STENCIL_OP_DECR_SAT;
        case GPUStencilOperation::INCREMENT_WRAP:
            return D3D12_STENCIL_OP_INCR;
        case GPUStencilOperation::DECREMENT_WRAP:
            return D3D12_STENCIL_OP_DECR;
        default:
            return D3D12_STENCIL_OP_KEEP;
    }
}

D3D12_BLEND_OP d3d12enum(GPUBlendOperation op)
{
    switch (op) {
        case GPUBlendOperation::ADD:
            return D3D12_BLEND_OP_ADD;
        case GPUBlendOperation::SUBTRACT:
            return D3D12_BLEND_OP_SUBTRACT;
        case GPUBlendOperation::REVERSE_SUBTRACT:
            return D3D12_BLEND_OP_REV_SUBTRACT;
        case GPUBlendOperation::MIN:
            return D3D12_BLEND_OP_MIN;
        case GPUBlendOperation::MAX:
            return D3D12_BLEND_OP_MAX;
        default:
            return D3D12_BLEND_OP_ADD;
    }
}

D3D12_BLEND d3d12enum(GPUBlendFactor factor)
{
    switch (factor) {
        case GPUBlendFactor::ZERO:
            return D3D12_BLEND_ZERO;
        case GPUBlendFactor::ONE:
            return D3D12_BLEND_ONE;
        case GPUBlendFactor::SRC:
            return D3D12_BLEND_SRC_COLOR;
        case GPUBlendFactor::ONE_MINUS_SRC:
            return D3D12_BLEND_INV_SRC_COLOR;
        case GPUBlendFactor::SRC_ALPHA:
            return D3D12_BLEND_SRC_ALPHA;
        case GPUBlendFactor::ONE_MINUS_SRC_ALPHA:
            return D3D12_BLEND_INV_SRC_ALPHA;
        case GPUBlendFactor::DST:
            return D3D12_BLEND_DEST_COLOR;
        case GPUBlendFactor::ONE_MINUS_DST:
            return D3D12_BLEND_INV_DEST_COLOR;
        case GPUBlendFactor::DST_ALPHA:
            return D3D12_BLEND_DEST_ALPHA;
        case GPUBlendFactor::ONE_MINUS_DST_ALPHA:
            return D3D12_BLEND_INV_DEST_ALPHA;
        case GPUBlendFactor::SRC_ALPHA_SATURATED:
            return D3D12_BLEND_SRC_ALPHA_SAT;
        case GPUBlendFactor::CONSTANT:
            return D3D12_BLEND_BLEND_FACTOR;
        case GPUBlendFactor::ONE_MINUS_CONSTANT:
            return D3D12_BLEND_INV_BLEND_FACTOR;
        default:
            return D3D12_BLEND_ONE;
    }
}

DXGI_FORMAT d3d12enum(GPUIndexFormat format)
{
    switch (format) {
        case GPUIndexFormat::UINT16:
            return DXGI_FORMAT_R16_UINT;
        case GPUIndexFormat::UINT32:
            return DXGI_FORMAT_R32_UINT;
        default:
            return DXGI_FORMAT_R32_UINT;
    }
}

D3D12_BARRIER_LAYOUT d3d12enum(GPUBarrierLayout layout)
{
    switch (layout) {
        case GPUBarrierLayout::UNDEFINED:
            return D3D12_BARRIER_LAYOUT_UNDEFINED;
        case GPUBarrierLayout::COMMON:
            return D3D12_BARRIER_LAYOUT_COMMON;
        case GPUBarrierLayout::PRESENT:
            return D3D12_BARRIER_LAYOUT_PRESENT;
        case GPUBarrierLayout::GENERIC_READ:
            return D3D12_BARRIER_LAYOUT_GENERIC_READ;
        case GPUBarrierLayout::RENDER_TARGET:
            return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
        case GPUBarrierLayout::UNORDERED_ACCESS:
            return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
        case GPUBarrierLayout::DEPTH_STENCIL_WRITE:
            return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
        case GPUBarrierLayout::DEPTH_STENCIL_READ:
            return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ;
        case GPUBarrierLayout::SHADER_RESOURCE:
            return D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
        case GPUBarrierLayout::COPY_SOURCE:
            return D3D12_BARRIER_LAYOUT_COPY_SOURCE;
        case GPUBarrierLayout::COPY_DEST:
            return D3D12_BARRIER_LAYOUT_COPY_DEST;
        case GPUBarrierLayout::RESOLVE_SOURCE:
            return D3D12_BARRIER_LAYOUT_RESOLVE_SOURCE;
        case GPUBarrierLayout::RESOLVE_DEST:
            return D3D12_BARRIER_LAYOUT_RESOLVE_DEST;
        case GPUBarrierLayout::SHADING_RATE_SOURCE:
            return D3D12_BARRIER_LAYOUT_SHADING_RATE_SOURCE;
        case GPUBarrierLayout::VIDEO_DECODE_READ:
            return D3D12_BARRIER_LAYOUT_VIDEO_DECODE_READ;
        case GPUBarrierLayout::VIDEO_DECODE_WRITE:
            return D3D12_BARRIER_LAYOUT_VIDEO_DECODE_WRITE;
        case GPUBarrierLayout::VIDEO_PROCESS_READ:
            return D3D12_BARRIER_LAYOUT_VIDEO_PROCESS_READ;
        case GPUBarrierLayout::VIDEO_PROCESS_WRITE:
            return D3D12_BARRIER_LAYOUT_VIDEO_PROCESS_WRITE;
        case GPUBarrierLayout::VIDEO_ENCODE_READ:
            return D3D12_BARRIER_LAYOUT_VIDEO_ENCODE_READ;
        case GPUBarrierLayout::VIDEO_ENCODE_WRITE:
            return D3D12_BARRIER_LAYOUT_VIDEO_ENCODE_WRITE;
        case GPUBarrierLayout::DIRECT_QUEUE_COMMON:
            return D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_COMMON;
        case GPUBarrierLayout::DIRECT_QUEUE_GENERIC_READ:
            return D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_GENERIC_READ;
        case GPUBarrierLayout::DIRECT_QUEUE_UNORDERED_ACCESS:
            return D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_UNORDERED_ACCESS;
        case GPUBarrierLayout::DIRECT_QUEUE_SHADER_RESOURCE:
            return D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_SHADER_RESOURCE;
        case GPUBarrierLayout::DIRECT_QUEUE_COPY_SOURCE:
            return D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_COPY_SOURCE;
        case GPUBarrierLayout::DIRECT_QUEUE_COPY_DEST:
            return D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_COPY_DEST;
        case GPUBarrierLayout::COMPUTE_QUEUE_COMMON:
            return D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_COMMON;
        case GPUBarrierLayout::COMPUTE_QUEUE_GENERIC_READ:
            return D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_GENERIC_READ;
        case GPUBarrierLayout::COMPUTE_QUEUE_UNORDERED_ACCESS:
            return D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_UNORDERED_ACCESS;
        case GPUBarrierLayout::COMPUTE_QUEUE_SHADER_RESOURCE:
            return D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_SHADER_RESOURCE;
        case GPUBarrierLayout::COMPUTE_QUEUE_COPY_SOURCE:
            return D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_COPY_SOURCE;
        case GPUBarrierLayout::COMPUTE_QUEUE_COPY_DEST:
            return D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_COPY_DEST;
        default:
            // handle invalid enum value - could throw or return undefined
            return D3D12_BARRIER_LAYOUT_UNDEFINED;
    }
}

D3D12_BARRIER_SYNC d3d12enum(GPUBarrierSyncFlags sync)
{
    D3D12_BARRIER_SYNC result = D3D12_BARRIER_SYNC_NONE;

    // clang-format off
    if (sync.contains(GPUBarrierSync::ALL))                          result |= D3D12_BARRIER_SYNC_ALL;
    if (sync.contains(GPUBarrierSync::DRAW))                         result |= D3D12_BARRIER_SYNC_DRAW;
    if (sync.contains(GPUBarrierSync::VERTEX_SHADING))               result |= D3D12_BARRIER_SYNC_VERTEX_SHADING;
    if (sync.contains(GPUBarrierSync::PIXEL_SHADING))                result |= D3D12_BARRIER_SYNC_PIXEL_SHADING;
    if (sync.contains(GPUBarrierSync::DEPTH_STENCIL))                result |= D3D12_BARRIER_SYNC_DEPTH_STENCIL;
    if (sync.contains(GPUBarrierSync::RENDER_TARGET))                result |= D3D12_BARRIER_SYNC_RENDER_TARGET;
    if (sync.contains(GPUBarrierSync::COMPUTE))                      result |= D3D12_BARRIER_SYNC_COMPUTE_SHADING;
    if (sync.contains(GPUBarrierSync::RAYTRACING))                   result |= D3D12_BARRIER_SYNC_RAYTRACING;
    if (sync.contains(GPUBarrierSync::COPY))                         result |= D3D12_BARRIER_SYNC_COPY;
    if (sync.contains(GPUBarrierSync::RESOLVE))                      result |= D3D12_BARRIER_SYNC_RESOLVE;
    if (sync.contains(GPUBarrierSync::EXECUTE_INDIRECT))             result |= D3D12_BARRIER_SYNC_EXECUTE_INDIRECT;
    if (sync.contains(GPUBarrierSync::ALL_SHADING))                  result |= D3D12_BARRIER_SYNC_ALL_SHADING;
    if (sync.contains(GPUBarrierSync::VIDEO_DECODE))                 result |= D3D12_BARRIER_SYNC_VIDEO_DECODE;
    if (sync.contains(GPUBarrierSync::VIDEO_ENCODE))                 result |= D3D12_BARRIER_SYNC_VIDEO_ENCODE;
    if (sync.contains(GPUBarrierSync::ACCELERATION_STRUCTURE_BUILD)) result |= D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE;
    if (sync.contains(GPUBarrierSync::ACCELERATION_STRUCTURE_COPY))  result |= D3D12_BARRIER_SYNC_COPY_RAYTRACING_ACCELERATION_STRUCTURE;
    // clang-format on

    return result;
}

auto d3d12enum(GPUBarrierAccessFlags access) -> D3D12_BARRIER_ACCESS
{
    D3D12_BARRIER_ACCESS result = D3D12_BARRIER_ACCESS_COMMON;

    // clang-format off
    if (access.contains(GPUBarrierAccess::VERTEX_BUFFER))                result |= D3D12_BARRIER_ACCESS_VERTEX_BUFFER;
    if (access.contains(GPUBarrierAccess::INDEX_BUFFER))                 result |= D3D12_BARRIER_ACCESS_INDEX_BUFFER;
    if (access.contains(GPUBarrierAccess::RENDER_TARGET))                result |= D3D12_BARRIER_ACCESS_RENDER_TARGET;
    if (access.contains(GPUBarrierAccess::UNORDERED_ACCESS))             result |= D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
    if (access.contains(GPUBarrierAccess::DEPTH_STENCIL_WRITE))          result |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
    if (access.contains(GPUBarrierAccess::DEPTH_STENCIL_READ))           result |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
    if (access.contains(GPUBarrierAccess::SHADER_RESOURCE))              result |= D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
    if (access.contains(GPUBarrierAccess::STREAM_OUTPUT))                result |= D3D12_BARRIER_ACCESS_STREAM_OUTPUT;
    if (access.contains(GPUBarrierAccess::INDIRECT_ARGUMENT))            result |= D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT;
    if (access.contains(GPUBarrierAccess::COPY_DEST))                    result |= D3D12_BARRIER_ACCESS_COPY_DEST;
    if (access.contains(GPUBarrierAccess::COPY_SOURCE))                  result |= D3D12_BARRIER_ACCESS_COPY_SOURCE;
    if (access.contains(GPUBarrierAccess::RESOLVE_DEST))                 result |= D3D12_BARRIER_ACCESS_RESOLVE_DEST;
    if (access.contains(GPUBarrierAccess::RESOLVE_SOURCE))               result |= D3D12_BARRIER_ACCESS_RESOLVE_SOURCE;
    if (access.contains(GPUBarrierAccess::ACCELERATION_STRUCTURE_READ))  result |= D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ;
    if (access.contains(GPUBarrierAccess::ACCELERATION_STRUCTURE_WRITE)) result |= D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE;
    if (access.contains(GPUBarrierAccess::SHADING_RATE_SOURCE))          result |= D3D12_BARRIER_ACCESS_SHADING_RATE_SOURCE;
    if (access.contains(GPUBarrierAccess::VIDEO_DECODE_READ))            result |= D3D12_BARRIER_ACCESS_VIDEO_DECODE_READ;
    if (access.contains(GPUBarrierAccess::VIDEO_DECODE_WRITE))           result |= D3D12_BARRIER_ACCESS_VIDEO_DECODE_WRITE;
    if (access.contains(GPUBarrierAccess::VIDEO_PROCESS_READ))           result |= D3D12_BARRIER_ACCESS_VIDEO_PROCESS_READ;
    if (access.contains(GPUBarrierAccess::VIDEO_PROCESS_WRITE))          result |= D3D12_BARRIER_ACCESS_VIDEO_PROCESS_WRITE;
    if (access.contains(GPUBarrierAccess::VIDEO_ENCODE_READ))            result |= D3D12_BARRIER_ACCESS_VIDEO_ENCODE_READ;
    if (access.contains(GPUBarrierAccess::VIDEO_ENCODE_WRITE))           result |= D3D12_BARRIER_ACCESS_VIDEO_ENCODE_WRITE;
    if (access.contains(GPUBarrierAccess::NO_ACCESS))                    result |= D3D12_BARRIER_ACCESS_NO_ACCESS;
    // clang-format on

    return result;
}

auto d3d12enum(GPUBVHFlags flags) -> D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS
{
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS result = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
    // clang-format off
    if (flags.contains(GPUBVHFlag::ALLOW_UPDATE))      result |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
    if (flags.contains(GPUBVHFlag::ALLOW_COMPACTION))  result |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_COMPACTION;
    if (flags.contains(GPUBVHFlag::PREFER_FAST_BUILD)) result |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
    if (flags.contains(GPUBVHFlag::PREFER_FAST_TRACE)) result |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    if (flags.contains(GPUBVHFlag::LOW_MEMORY))        result |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_MINIMIZE_MEMORY;
    // clang-format on
    return result;
}

auto d3d12enum(GPUBVHGeometryFlags flags) -> D3D12_RAYTRACING_GEOMETRY_FLAGS
{
    D3D12_RAYTRACING_GEOMETRY_FLAGS result = D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
    // clang-format off
    if (flags.contains(GPUBVHGeometryFlag::BVH_OPAQUE))                      result |= D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
    if (flags.contains(GPUBVHGeometryFlag::NO_DUPLICATE_ANY_HIT_INVOCATION)) result |= D3D12_RAYTRACING_GEOMETRY_FLAG_NO_DUPLICATE_ANYHIT_INVOCATION;
    // clang-format on
    return result;
}

uint size_of(DXGI_FORMAT format)
{
    switch (format) {
        // 8-bit formats
        case DXGI_FORMAT_R8_UNORM:
        case DXGI_FORMAT_R8_SNORM:
        case DXGI_FORMAT_R8_UINT:
        case DXGI_FORMAT_R8_SINT:
        case DXGI_FORMAT_A8_UNORM:
            return 1;

        // 16-bit formats
        case DXGI_FORMAT_R16_UNORM:
        case DXGI_FORMAT_R16_SNORM:
        case DXGI_FORMAT_R16_UINT:
        case DXGI_FORMAT_R16_SINT:
        case DXGI_FORMAT_R16_FLOAT:
        case DXGI_FORMAT_R8G8_UNORM:
        case DXGI_FORMAT_R8G8_SNORM:
        case DXGI_FORMAT_R8G8_UINT:
        case DXGI_FORMAT_R8G8_SINT:
        case DXGI_FORMAT_D16_UNORM:
        case DXGI_FORMAT_B5G6R5_UNORM:
        case DXGI_FORMAT_B5G5R5A1_UNORM:
        case DXGI_FORMAT_B4G4R4A4_UNORM:
            return 2;

        // 24-bit formats
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_R8G8B8A8_SNORM:
        case DXGI_FORMAT_R8G8B8A8_UINT:
        case DXGI_FORMAT_R8G8B8A8_SINT:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8X8_UNORM:
        case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
        case DXGI_FORMAT_R16G16_UNORM:
        case DXGI_FORMAT_R16G16_SNORM:
        case DXGI_FORMAT_R16G16_UINT:
        case DXGI_FORMAT_R16G16_SINT:
        case DXGI_FORMAT_R16G16_FLOAT:
        case DXGI_FORMAT_R32_UINT:
        case DXGI_FORMAT_R32_SINT:
        case DXGI_FORMAT_R32_FLOAT:
        case DXGI_FORMAT_D32_FLOAT:
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        case DXGI_FORMAT_R10G10B10A2_UNORM:
        case DXGI_FORMAT_R10G10B10A2_UINT:
        case DXGI_FORMAT_R11G11B10_FLOAT:
        case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
            return 4;

        // 64-bit formats
        case DXGI_FORMAT_R16G16B16A16_UNORM:
        case DXGI_FORMAT_R16G16B16A16_SNORM:
        case DXGI_FORMAT_R16G16B16A16_UINT:
        case DXGI_FORMAT_R16G16B16A16_SINT:
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        case DXGI_FORMAT_R32G32_UINT:
        case DXGI_FORMAT_R32G32_SINT:
        case DXGI_FORMAT_R32G32_FLOAT:
            return 8;

        // 96-bit formats
        case DXGI_FORMAT_R32G32B32_UINT:
        case DXGI_FORMAT_R32G32B32_SINT:
        case DXGI_FORMAT_R32G32B32_FLOAT:
            return 12;

        // 128-bit formats
        case DXGI_FORMAT_R32G32B32A32_UINT:
        case DXGI_FORMAT_R32G32B32A32_SINT:
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
            return 16;

        // compressed formats - these return block size, not per-texel
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:
        case DXGI_FORMAT_BC4_UNORM:
        case DXGI_FORMAT_BC4_SNORM:
            return 8; // 4x4 block = 64 bits = 8 bytes

        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC2_UNORM_SRGB:
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB:
        case DXGI_FORMAT_BC5_UNORM:
        case DXGI_FORMAT_BC5_SNORM:
        case DXGI_FORMAT_BC6H_UF16:
        case DXGI_FORMAT_BC6H_SF16:
        case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC7_UNORM_SRGB:
            return 16; // 4x4 block = 128 bits = 16 bytes

        // special cases
        case DXGI_FORMAT_UNKNOWN:
        default:
            return 0; // unknown or unsupported format
    }
}
