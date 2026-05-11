#include "MetalUtils.h"
#include <stdexcept>

using namespace lyra;

// present Mode (for swapchain)
auto mtlenum(GPUPresentMode mode) -> MTLPixelFormat
{
    // Note: Metal doesn't have direct present mode equivalents
    // This is handled via CAMetalLayer's display sync properties
    // Returning a default format for compatibility
    return MTLPixelFormatBGRA8Unorm;
}

// composite alpha mode
auto mtlenum(GPUCompositeAlphaMode mode) -> uint32_t
{
    // Metal handles alpha blending differently
    // Return 0 for now (opaque)
    return 0;
}

// color space
auto mtlenum(GPUColorSpace space) -> uint32_t
{
    // Metal uses CGColorSpace, not directly mappable
    return 0;
}

// blend operation
auto mtlenum(GPUBlendOperation op) -> MTLBlendOperation
{
    // clang-format off
    switch (op) {
        case GPUBlendOperation::ADD:              return MTLBlendOperationAdd;
        case GPUBlendOperation::SUBTRACT:         return MTLBlendOperationSubtract;
        case GPUBlendOperation::REVERSE_SUBTRACT: return MTLBlendOperationReverseSubtract;
        case GPUBlendOperation::MIN:              return MTLBlendOperationMin;
        case GPUBlendOperation::MAX:              return MTLBlendOperationMax;
        default:
            throw std::runtime_error("Invalid GPUBlendOperation");
    }
    // clang-format on
}

// blend factor
auto mtlenum(GPUBlendFactor factor) -> MTLBlendFactor
{
    // clang-format off
    switch (factor) {
        case GPUBlendFactor::ZERO:                     return MTLBlendFactorZero;
        case GPUBlendFactor::ONE:                      return MTLBlendFactorOne;
        case GPUBlendFactor::SRC:                      return MTLBlendFactorSourceColor;
        case GPUBlendFactor::ONE_MINUS_SRC:            return MTLBlendFactorOneMinusSourceColor;
        case GPUBlendFactor::SRC_ALPHA:                return MTLBlendFactorSourceAlpha;
        case GPUBlendFactor::ONE_MINUS_SRC_ALPHA:      return MTLBlendFactorOneMinusSourceAlpha;
        case GPUBlendFactor::DST:                      return MTLBlendFactorDestinationColor;
        case GPUBlendFactor::ONE_MINUS_DST:            return MTLBlendFactorOneMinusDestinationColor;
        case GPUBlendFactor::DST_ALPHA:                return MTLBlendFactorDestinationAlpha;
        case GPUBlendFactor::ONE_MINUS_DST_ALPHA:      return MTLBlendFactorOneMinusDestinationAlpha;
        case GPUBlendFactor::SRC_ALPHA_SATURATED:      return MTLBlendFactorSourceAlphaSaturated;
        case GPUBlendFactor::CONSTANT:                 return MTLBlendFactorBlendColor;
        case GPUBlendFactor::ONE_MINUS_CONSTANT:       return MTLBlendFactorOneMinusBlendColor;
        case GPUBlendFactor::SRC1:                     return MTLBlendFactorSource1Color;
        case GPUBlendFactor::ONE_MINUS_SRC1:           return MTLBlendFactorOneMinusSource1Color;
        case GPUBlendFactor::SRC1_ALPHA:               return MTLBlendFactorSource1Alpha;
        case GPUBlendFactor::ONE_MINUS_SRC1_ALPHA:     return MTLBlendFactorOneMinusSource1Alpha;
        default:
            throw std::runtime_error("Invalid GPUBlendFactor");
    }
    // clang-format on
}

// load operation
auto mtlenum(GPULoadOp op) -> MTLLoadAction
{
    // clang-format off
    switch (op) {
        case GPULoadOp::LOAD:  return MTLLoadActionLoad;
        case GPULoadOp::CLEAR: return MTLLoadActionClear;
        default:
            throw std::runtime_error("Invalid GPULoadOp");
    }
    // clang-format on
}

// store operation
auto mtlenum(GPUStoreOp op) -> MTLStoreAction
{
    // clang-format off
    switch (op) {
        case GPUStoreOp::STORE:   return MTLStoreActionStore;
        case GPUStoreOp::DISCARD: return MTLStoreActionDontCare;
        default:
            throw std::runtime_error("Invalid GPUStoreOp");
    }
    // clang-format on
}

// query type
auto mtlenum(GPUQueryType query) -> uint32_t
{
    // Metal query types are different, handled in query implementation
    return 0;
}

// texture dimension
auto mtlenum(GPUTextureDimension dim) -> MTLTextureType
{
    // clang-format off
    switch (dim) {
        case GPUTextureDimension::x1D: return MTLTextureType1D;
        case GPUTextureDimension::x2D: return MTLTextureType2D;
        case GPUTextureDimension::x3D: return MTLTextureType3D;
        default:
            throw std::runtime_error("Invalid GPUTextureDimension");
    }
    // clang-format on
}

// texture view dimension
auto mtlenum(GPUTextureViewDimension dim) -> MTLTextureType
{
    // clang-format off
    switch (dim) {
        case GPUTextureViewDimension::x1D:        return MTLTextureType1D;
        case GPUTextureViewDimension::x2D:        return MTLTextureType2D;
        case GPUTextureViewDimension::x2D_ARRAY:  return MTLTextureType2DArray;
        case GPUTextureViewDimension::CUBE:       return MTLTextureTypeCube;
        case GPUTextureViewDimension::CUBE_ARRAY: return MTLTextureTypeCubeArray;
        case GPUTextureViewDimension::x3D:        return MTLTextureType3D;
        default:
            throw std::runtime_error("Invalid GPUTextureViewDimension");
    }
    // clang-format on
}

// sampler address mode
auto mtlenum(GPUAddressMode mode) -> MTLSamplerAddressMode
{
    // clang-format off
    switch (mode) {
        case GPUAddressMode::REPEAT:          return MTLSamplerAddressModeRepeat;
        case GPUAddressMode::MIRROR_REPEAT:   return MTLSamplerAddressModeMirrorRepeat;
        case GPUAddressMode::CLAMP_TO_EDGE:   return MTLSamplerAddressModeClampToEdge;
        default:
            throw std::runtime_error("Invalid GPUAddressMode");
    }
    // clang-format on
}

// filter mode
auto mtlenum(GPUFilterMode filter) -> MTLSamplerMinMagFilter
{
    // clang-format off
    switch (filter) {
        case GPUFilterMode::NEAREST: return MTLSamplerMinMagFilterNearest;
        case GPUFilterMode::LINEAR:  return MTLSamplerMinMagFilterLinear;
        default:
            throw std::runtime_error("Invalid GPUFilterMode");
    }
    // clang-format on
}

// mipmap filter mode
auto mtlenum(GPUMipmapFilterMode filter) -> MTLSamplerMipFilter
{
    // clang-format off
    switch (filter) {
        case GPUMipmapFilterMode::NEAREST: return MTLSamplerMipFilterNearest;
        case GPUMipmapFilterMode::LINEAR:  return MTLSamplerMipFilterLinear;
        default:
            throw std::runtime_error("Invalid GPUMipmapFilterMode");
    }
    // clang-format on
}

// compare function
auto mtlenum(GPUCompareFunction op) -> MTLCompareFunction
{
    // clang-format off
    switch (op) {
        case GPUCompareFunction::NEVER:         return MTLCompareFunctionNever;
        case GPUCompareFunction::LESS:          return MTLCompareFunctionLess;
        case GPUCompareFunction::EQUAL:         return MTLCompareFunctionEqual;
        case GPUCompareFunction::LESS_EQUAL:    return MTLCompareFunctionLessEqual;
        case GPUCompareFunction::GREATER:       return MTLCompareFunctionGreater;
        case GPUCompareFunction::NOT_EQUAL:     return MTLCompareFunctionNotEqual;
        case GPUCompareFunction::GREATER_EQUAL: return MTLCompareFunctionGreaterEqual;
        case GPUCompareFunction::ALWAYS:        return MTLCompareFunctionAlways;
        default:
            throw std::runtime_error("Invalid GPUCompareFunction");
    }
    // clang-format on
}

// stencil operation
auto mtlenum(GPUStencilOperation op) -> MTLStencilOperation
{
    // clang-format off
    switch (op) {
        case GPUStencilOperation::KEEP:            return MTLStencilOperationKeep;
        case GPUStencilOperation::ZERO:            return MTLStencilOperationZero;
        case GPUStencilOperation::REPLACE:         return MTLStencilOperationReplace;
        case GPUStencilOperation::INVERT:          return MTLStencilOperationInvert;
        case GPUStencilOperation::INCREMENT_CLAMP: return MTLStencilOperationIncrementClamp;
        case GPUStencilOperation::DECREMENT_CLAMP: return MTLStencilOperationDecrementClamp;
        case GPUStencilOperation::INCREMENT_WRAP:  return MTLStencilOperationIncrementWrap;
        case GPUStencilOperation::DECREMENT_WRAP:  return MTLStencilOperationDecrementWrap;
        default:
            throw std::runtime_error("Invalid GPUStencilOperation");
    }
    // clang-format on
}

// front face
auto mtlenum(GPUFrontFace winding) -> MTLWinding
{
    // clang-format off
    switch (winding) {
        case GPUFrontFace::CCW: return MTLWindingCounterClockwise;
        case GPUFrontFace::CW:  return MTLWindingClockwise;
        default:
            throw std::runtime_error("Invalid GPUFrontFace");
    }
    // clang-format on
}

// cull mode
auto mtlenum(GPUCullMode culling) -> MTLCullMode
{
    // clang-format off
    switch (culling) {
        case GPUCullMode::NONE:  return MTLCullModeNone;
        case GPUCullMode::FRONT: return MTLCullModeFront;
        case GPUCullMode::BACK:  return MTLCullModeBack;
        default:
            throw std::runtime_error("Invalid GPUCullMode");
    }
    // clang-format on
}

// primitive topology
auto mtlenum(GPUPrimitiveTopology topology) -> MTLPrimitiveType
{
    // clang-format off
    switch (topology) {
        case GPUPrimitiveTopology::POINT_LIST:     return MTLPrimitiveTypePoint;
        case GPUPrimitiveTopology::LINE_LIST:      return MTLPrimitiveTypeLine;
        case GPUPrimitiveTopology::LINE_STRIP:     return MTLPrimitiveTypeLineStrip;
        case GPUPrimitiveTopology::TRIANGLE_LIST:  return MTLPrimitiveTypeTriangle;
        case GPUPrimitiveTopology::TRIANGLE_STRIP: return MTLPrimitiveTypeTriangleStrip;
        default:
            throw std::runtime_error("Invalid GPUPrimitiveTopology");
    }
    // clang-format on
}

// vertex step mode
auto mtlenum(GPUVertexStepMode step) -> MTLVertexStepFunction
{
    // clang-format off
    switch (step) {
        case GPUVertexStepMode::VERTEX:   return MTLVertexStepFunctionPerVertex;
        case GPUVertexStepMode::INSTANCE: return MTLVertexStepFunctionPerInstance;
        default:
            throw std::runtime_error("Invalid GPUVertexStepMode");
    }
    // clang-format on
}

// index format
auto mtlenum(GPUIndexFormat format) -> MTLIndexType
{
    // clang-format off
    switch (format) {
        case GPUIndexFormat::UINT16: return MTLIndexTypeUInt16;
        case GPUIndexFormat::UINT32: return MTLIndexTypeUInt32;
        default:
            throw std::runtime_error("Invalid GPUIndexFormat");
    }
    // clang-format on
}

// vertex format
auto mtlenum(GPUVertexFormat format) -> MTLVertexFormat
{
    // clang-format off
    switch (format) {
        case GPUVertexFormat::UINT8x2:    return MTLVertexFormatUChar2;
        case GPUVertexFormat::UINT8x4:    return MTLVertexFormatUChar4;
        case GPUVertexFormat::SINT8x2:    return MTLVertexFormatChar2;
        case GPUVertexFormat::SINT8x4:    return MTLVertexFormatChar4;
        case GPUVertexFormat::UNORM8x2:   return MTLVertexFormatUChar2Normalized;
        case GPUVertexFormat::UNORM8x4:   return MTLVertexFormatUChar4Normalized;
        case GPUVertexFormat::SNORM8x2:   return MTLVertexFormatChar2Normalized;
        case GPUVertexFormat::SNORM8x4:   return MTLVertexFormatChar4Normalized;
        case GPUVertexFormat::UINT16x2:   return MTLVertexFormatUShort2;
        case GPUVertexFormat::UINT16x4:   return MTLVertexFormatUShort4;
        case GPUVertexFormat::SINT16x2:   return MTLVertexFormatShort2;
        case GPUVertexFormat::SINT16x4:   return MTLVertexFormatShort4;
        case GPUVertexFormat::UNORM16x2:  return MTLVertexFormatUShort2Normalized;
        case GPUVertexFormat::UNORM16x4:  return MTLVertexFormatUShort4Normalized;
        case GPUVertexFormat::SNORM16x2:  return MTLVertexFormatShort2Normalized;
        case GPUVertexFormat::SNORM16x4:  return MTLVertexFormatShort4Normalized;
        case GPUVertexFormat::FLOAT16x2:  return MTLVertexFormatHalf2;
        case GPUVertexFormat::FLOAT16x4:  return MTLVertexFormatHalf4;
        case GPUVertexFormat::FLOAT32:    return MTLVertexFormatFloat;
        case GPUVertexFormat::FLOAT32x2:  return MTLVertexFormatFloat2;
        case GPUVertexFormat::FLOAT32x3:  return MTLVertexFormatFloat3;
        case GPUVertexFormat::FLOAT32x4:  return MTLVertexFormatFloat4;
        case GPUVertexFormat::UINT32:     return MTLVertexFormatUInt;
        case GPUVertexFormat::UINT32x2:   return MTLVertexFormatUInt2;
        case GPUVertexFormat::UINT32x3:   return MTLVertexFormatUInt3;
        case GPUVertexFormat::UINT32x4:   return MTLVertexFormatUInt4;
        case GPUVertexFormat::SINT32:     return MTLVertexFormatInt;
        case GPUVertexFormat::SINT32x2:   return MTLVertexFormatInt2;
        case GPUVertexFormat::SINT32x3:   return MTLVertexFormatInt3;
        case GPUVertexFormat::SINT32x4:   return MTLVertexFormatInt4;
        default:
            throw std::runtime_error("Invalid or unsupported GPUVertexFormat");
    }
    // clang-format on
}

// texture format (comprehensive mapping)
auto mtlenum(GPUTextureFormat format) -> MTLPixelFormat
{
    // clang-format off
    switch (format) {
        // 8-bit formats
        case GPUTextureFormat::R8UNORM:               return MTLPixelFormatR8Unorm;
        case GPUTextureFormat::R8SNORM:               return MTLPixelFormatR8Snorm;
        case GPUTextureFormat::R8UINT:                return MTLPixelFormatR8Uint;
        case GPUTextureFormat::R8SINT:                return MTLPixelFormatR8Sint;

        // 16-bit formats
        case GPUTextureFormat::R16UINT:               return MTLPixelFormatR16Uint;
        case GPUTextureFormat::R16SINT:               return MTLPixelFormatR16Sint;
        case GPUTextureFormat::RG8UNORM:              return MTLPixelFormatRG8Unorm;
        case GPUTextureFormat::RG8SNORM:              return MTLPixelFormatRG8Snorm;
        case GPUTextureFormat::RG8UINT:               return MTLPixelFormatRG8Uint;
        case GPUTextureFormat::RG8SINT:               return MTLPixelFormatRG8Sint;

        // 32-bit formats
        case GPUTextureFormat::R32UINT:               return MTLPixelFormatR32Uint;
        case GPUTextureFormat::R32SINT:               return MTLPixelFormatR32Sint;
        case GPUTextureFormat::R32FLOAT:              return MTLPixelFormatR32Float;
        case GPUTextureFormat::RG16UINT:              return MTLPixelFormatRG16Uint;
        case GPUTextureFormat::RG16SINT:              return MTLPixelFormatRG16Sint;
        case GPUTextureFormat::RG16FLOAT:             return MTLPixelFormatRG16Float;
        case GPUTextureFormat::RGBA8UNORM:            return MTLPixelFormatRGBA8Unorm;
        case GPUTextureFormat::RGBA8UNORM_SRGB:       return MTLPixelFormatRGBA8Unorm_sRGB;
        case GPUTextureFormat::RGBA8SNORM:            return MTLPixelFormatRGBA8Snorm;
        case GPUTextureFormat::RGBA8UINT:             return MTLPixelFormatRGBA8Uint;
        case GPUTextureFormat::RGBA8SINT:             return MTLPixelFormatRGBA8Sint;
        case GPUTextureFormat::BGRA8UNORM:            return MTLPixelFormatBGRA8Unorm;
        case GPUTextureFormat::BGRA8UNORM_SRGB:       return MTLPixelFormatBGRA8Unorm_sRGB;

        // Packed 32-bit formats
        case GPUTextureFormat::RGB10A2UNORM:          return MTLPixelFormatRGB10A2Unorm;
        case GPUTextureFormat::RG11B10UFLOAT:         return MTLPixelFormatRG11B10Float;
        case GPUTextureFormat::RGB9E5UFLOAT:          return MTLPixelFormatRGB9E5Float;

        // 64-bit formats
        case GPUTextureFormat::RG32UINT:              return MTLPixelFormatRG32Uint;
        case GPUTextureFormat::RG32SINT:              return MTLPixelFormatRG32Sint;
        case GPUTextureFormat::RG32FLOAT:             return MTLPixelFormatRG32Float;
        case GPUTextureFormat::RGBA16UINT:            return MTLPixelFormatRGBA16Uint;
        case GPUTextureFormat::RGBA16SINT:            return MTLPixelFormatRGBA16Sint;
        case GPUTextureFormat::RGBA16FLOAT:           return MTLPixelFormatRGBA16Float;

        // 128-bit formats
        case GPUTextureFormat::RGBA32UINT:            return MTLPixelFormatRGBA32Uint;
        case GPUTextureFormat::RGBA32SINT:            return MTLPixelFormatRGBA32Sint;
        case GPUTextureFormat::RGBA32FLOAT:           return MTLPixelFormatRGBA32Float;

        // depth/stencil formats
        case GPUTextureFormat::DEPTH16UNORM:          return MTLPixelFormatDepth16Unorm;
        case GPUTextureFormat::DEPTH32FLOAT:          return MTLPixelFormatDepth32Float;
        case GPUTextureFormat::DEPTH24PLUS:           return MTLPixelFormatDepth32Float;  // Metal doesn't have 24-bit
        case GPUTextureFormat::DEPTH24PLUS_STENCIL8:  return MTLPixelFormatDepth32Float_Stencil8;
        case GPUTextureFormat::DEPTH32FLOAT_STENCIL8: return MTLPixelFormatDepth32Float_Stencil8;
        case GPUTextureFormat::STENCIL8:              return MTLPixelFormatStencil8;

        // BC compressed formats
        case GPUTextureFormat::BC1_RGBA_UNORM:        return MTLPixelFormatBC1_RGBA;
        case GPUTextureFormat::BC1_RGBA_UNORM_SRGB:   return MTLPixelFormatBC1_RGBA_sRGB;
        case GPUTextureFormat::BC2_RGBA_UNORM:        return MTLPixelFormatBC2_RGBA;
        case GPUTextureFormat::BC2_RGBA_UNORM_SRGB:   return MTLPixelFormatBC2_RGBA_sRGB;
        case GPUTextureFormat::BC3_RGBA_UNORM:        return MTLPixelFormatBC3_RGBA;
        case GPUTextureFormat::BC3_RGBA_UNORM_SRGB:   return MTLPixelFormatBC3_RGBA_sRGB;
        case GPUTextureFormat::BC4_R_UNORM:           return MTLPixelFormatBC4_RUnorm;
        case GPUTextureFormat::BC4_R_SNORM:           return MTLPixelFormatBC4_RSnorm;
        case GPUTextureFormat::BC5_RG_UNORM:          return MTLPixelFormatBC5_RGUnorm;
        case GPUTextureFormat::BC5_RG_SNORM:          return MTLPixelFormatBC5_RGSnorm;
        case GPUTextureFormat::BC6H_RGB_UFLOAT:       return MTLPixelFormatBC6H_RGBUfloat;
        case GPUTextureFormat::BC6H_RGB_FLOAT:        return MTLPixelFormatBC6H_RGBFloat;
        case GPUTextureFormat::BC7_RGBA_UNORM:        return MTLPixelFormatBC7_RGBAUnorm;
        case GPUTextureFormat::BC7_RGBA_UNORM_SRGB:   return MTLPixelFormatBC7_RGBAUnorm_sRGB;

        default:
            throw std::runtime_error("Invalid or unsupported GPUTextureFormat");
    }
    // clang-format on
}

// barrier layout (Metal doesn't have explicit layouts)
auto mtlenum(GPUBarrierLayout layout) -> uint32_t
{
    return 0; // Metal handles layout transitions automatically
}

// sample count
auto mtlenum(GPUIntegerCoordinate samples) -> NSUInteger
{
    return static_cast<NSUInteger>(samples);
}

// BLAS Type
auto mtlenum(GPUBlasType type) -> uint32_t
{
    return 0; // Handled in acceleration structure implementation
}

// BVH update mode
auto mtlenum(GPUBVHUpdateMode mode) -> uint32_t
{
    return 0; // Handled in acceleration structure implementation
}

// texture aspect
auto mtlenum(GPUTextureAspectFlags aspect) -> MTLTextureUsage
{
    // Aspect flags don't directly map to texture usage
    // This is used for texture views and barriers
    return MTLTextureUsageUnknown;
}

// color write mask
auto mtlenum(GPUColorWriteFlags color) -> MTLColorWriteMask
{
    // clang-format off
    MTLColorWriteMask mask = MTLColorWriteMaskNone;
    if (color.contains(GPUColorWrite::RED))   mask |= MTLColorWriteMaskRed;
    if (color.contains(GPUColorWrite::GREEN)) mask |= MTLColorWriteMaskGreen;
    if (color.contains(GPUColorWrite::BLUE))  mask |= MTLColorWriteMaskBlue;
    if (color.contains(GPUColorWrite::ALPHA)) mask |= MTLColorWriteMaskAlpha;
    // clang-format on
    return mask;
}

// buffer usage flags
auto mtlenum(GPUBufferUsageFlags usages) -> std::pair<MTLResourceOptions, MTLStorageMode>
{
    MTLResourceOptions options      = MTLResourceStorageModeShared;
    MTLStorageMode     storage_mode = MTLStorageModeShared;

    // Determine storage mode based on usage
    bool cpu_visible = (usages.contains(GPUBufferUsage::MAP_READ)) || (usages.contains(GPUBufferUsage::MAP_WRITE));

    if (cpu_visible) {
        // CPU-visible buffers use Shared mode
        storage_mode = MTLStorageModeShared;
        options      = MTLResourceStorageModeShared;
    } else {
        // GPU-only buffers use Private mode (fastest)
        storage_mode = MTLStorageModePrivate;
        options      = MTLResourceStorageModePrivate;
    }

    return std::make_pair(options, storage_mode);
}

// texture usage flags
auto mtlenum(GPUTextureUsageFlags usages) -> MTLTextureUsage
{
    MTLTextureUsage usage = MTLTextureUsageUnknown;

    // clang-format off
    if (usages.contains(GPUTextureUsage::COPY_SRC))          usage |= MTLTextureUsageShaderRead;
    if (usages.contains(GPUTextureUsage::COPY_DST))          usage |= MTLTextureUsageShaderWrite;
    if (usages.contains(GPUTextureUsage::TEXTURE_BINDING))   usage |= MTLTextureUsageShaderRead;
    if (usages.contains(GPUTextureUsage::STORAGE_BINDING))   usage |= MTLTextureUsageShaderWrite;
    if (usages.contains(GPUTextureUsage::RENDER_ATTACHMENT)) usage |= MTLTextureUsageRenderTarget;
    // clang-format on

    return usage;
}

// shader stage flags
auto mtlenum(GPUShaderStageFlags stages) -> uint32_t
{
    // Metal doesn't have direct stage flags
    // This is handled per-pipeline
    return 0;
}

// barrier Sync Flags
auto mtlenum(GPUBarrierSyncFlags flags) -> MTLBarrierScope
{
    MTLBarrierScope scope = 0;

    // map common sync points to Metal barrier scopes
    if (flags.contains(GPUBarrierSync::VERTEX_SHADING) ||
        flags.contains(GPUBarrierSync::PIXEL_SHADING) ||
        flags.contains(GPUBarrierSync::DRAW))
        scope |= MTLBarrierScopeRenderTargets;
    if (flags.contains(GPUBarrierSync::COMPUTE) ||
        flags.contains(GPUBarrierSync::RAYTRACING))
        scope |= MTLBarrierScopeBuffers | MTLBarrierScopeTextures;
    if (flags.contains(GPUBarrierSync::COPY) ||
        flags.contains(GPUBarrierSync::RESOLVE))
        scope |= MTLBarrierScopeBuffers | MTLBarrierScopeTextures;

    return scope;
}

// barrier Access Flags
auto mtlenum(GPUBarrierAccessFlags flags) -> uint32_t
{
    // Metal handles access tracking automatically
    return 0;
}

// BVH Flags
auto mtlenum(GPUBVHFlags flags) -> uint32_t
{
    // handled in acceleration structure implementation
    return 0;
}

// BVH Geometry Flags
auto mtlenum(GPUBVHGeometryFlags flags) -> uint32_t
{
    // handled in acceleration structure implementation
    return 0;
}

MTLStorageMode determine_texture_storage_mode(GPUTextureFormat format)
{
    // depth/stencil textures must be private on all macOS hardware
    if (is_depth_stencil_format(format)) {
        return MTLStorageModePrivate;
    }

    // for other textures, use shared memory on Apple Silicon and managed on Intel
    auto rhi = get_rhi();
    return rhi->has_unified_memory ? MTLStorageModeShared : MTLStorageModeManaged;
}

uint size_of(MTLPixelFormat format)
{
    switch (format) {
        // 8-bit formats
        case MTLPixelFormatR8Unorm:
        case MTLPixelFormatR8Unorm_sRGB:
        case MTLPixelFormatR8Snorm:
        case MTLPixelFormatR8Uint:
        case MTLPixelFormatR8Sint:
        case MTLPixelFormatStencil8:
            return 1;

        // 16-bit formats
        case MTLPixelFormatR16Unorm:
        case MTLPixelFormatR16Snorm:
        case MTLPixelFormatR16Uint:
        case MTLPixelFormatR16Sint:
        case MTLPixelFormatR16Float:
        case MTLPixelFormatRG8Unorm:
        case MTLPixelFormatRG8Unorm_sRGB:
        case MTLPixelFormatRG8Snorm:
        case MTLPixelFormatRG8Uint:
        case MTLPixelFormatRG8Sint:
        case MTLPixelFormatDepth16Unorm:
            return 2;

        // 32-bit formats
        case MTLPixelFormatR32Uint:
        case MTLPixelFormatR32Sint:
        case MTLPixelFormatR32Float:
        case MTLPixelFormatRG16Unorm:
        case MTLPixelFormatRG16Snorm:
        case MTLPixelFormatRG16Uint:
        case MTLPixelFormatRG16Sint:
        case MTLPixelFormatRG16Float:
        case MTLPixelFormatRGBA8Unorm:
        case MTLPixelFormatRGBA8Unorm_sRGB:
        case MTLPixelFormatRGBA8Snorm:
        case MTLPixelFormatRGBA8Uint:
        case MTLPixelFormatRGBA8Sint:
        case MTLPixelFormatBGRA8Unorm:
        case MTLPixelFormatBGRA8Unorm_sRGB:
        case MTLPixelFormatRGB10A2Unorm:
        case MTLPixelFormatRGB10A2Uint:
        case MTLPixelFormatRG11B10Float:
        case MTLPixelFormatRGB9E5Float:
        case MTLPixelFormatDepth32Float:
            return 4;

        // 64-bit formats
        case MTLPixelFormatRG32Uint:
        case MTLPixelFormatRG32Sint:
        case MTLPixelFormatRG32Float:
        case MTLPixelFormatRGBA16Unorm:
        case MTLPixelFormatRGBA16Snorm:
        case MTLPixelFormatRGBA16Uint:
        case MTLPixelFormatRGBA16Sint:
        case MTLPixelFormatRGBA16Float:
            return 8;

        // 128-bit formats
        case MTLPixelFormatRGBA32Uint:
        case MTLPixelFormatRGBA32Sint:
        case MTLPixelFormatRGBA32Float:
            return 16;

        // depth/stencil (Depth32Float_Stencil8 is 5 bytes, but usually 8 or padded)
        case MTLPixelFormatDepth32Float_Stencil8:
            return 8; // Metal uses 8 bytes for this format in buffer copies

        // BC compressed formats (all 4x4 blocks)
        case MTLPixelFormatBC1_RGBA:
        case MTLPixelFormatBC1_RGBA_sRGB:
        case MTLPixelFormatBC4_RUnorm:
        case MTLPixelFormatBC4_RSnorm:
            return 8; // 8 bytes per 4x4 block

        case MTLPixelFormatBC2_RGBA:
        case MTLPixelFormatBC2_RGBA_sRGB:
        case MTLPixelFormatBC3_RGBA:
        case MTLPixelFormatBC3_RGBA_sRGB:
        case MTLPixelFormatBC5_RGUnorm:
        case MTLPixelFormatBC5_RGSnorm:
        case MTLPixelFormatBC6H_RGBUfloat:
        case MTLPixelFormatBC6H_RGBFloat:
        case MTLPixelFormatBC7_RGBAUnorm:
        case MTLPixelFormatBC7_RGBAUnorm_sRGB:
            return 16; // 16 bytes per 4x4 block

        default:
            return 0;
    }
}

uint block_width(MTLPixelFormat format)
{
    switch (format) {
        case MTLPixelFormatBC1_RGBA:
        case MTLPixelFormatBC1_RGBA_sRGB:
        case MTLPixelFormatBC2_RGBA:
        case MTLPixelFormatBC2_RGBA_sRGB:
        case MTLPixelFormatBC3_RGBA:
        case MTLPixelFormatBC3_RGBA_sRGB:
        case MTLPixelFormatBC4_RUnorm:
        case MTLPixelFormatBC4_RSnorm:
        case MTLPixelFormatBC5_RGUnorm:
        case MTLPixelFormatBC5_RGSnorm:
        case MTLPixelFormatBC6H_RGBUfloat:
        case MTLPixelFormatBC6H_RGBFloat:
        case MTLPixelFormatBC7_RGBAUnorm:
        case MTLPixelFormatBC7_RGBAUnorm_sRGB:
            return 4;
        default:
            return 1;
    }
}

uint block_height(MTLPixelFormat format)
{
    switch (format) {
        case MTLPixelFormatBC1_RGBA:
        case MTLPixelFormatBC1_RGBA_sRGB:
        case MTLPixelFormatBC2_RGBA:
        case MTLPixelFormatBC2_RGBA_sRGB:
        case MTLPixelFormatBC3_RGBA:
        case MTLPixelFormatBC3_RGBA_sRGB:
        case MTLPixelFormatBC4_RUnorm:
        case MTLPixelFormatBC4_RSnorm:
        case MTLPixelFormatBC5_RGUnorm:
        case MTLPixelFormatBC5_RGSnorm:
        case MTLPixelFormatBC6H_RGBUfloat:
        case MTLPixelFormatBC6H_RGBFloat:
        case MTLPixelFormatBC7_RGBAUnorm:
        case MTLPixelFormatBC7_RGBAUnorm_sRGB:
            return 4;
        default:
            return 1;
    }
}
