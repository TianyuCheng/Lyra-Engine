#pragma once

#ifndef LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTTEXTURE_H
#define LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTTEXTURE_H

#include "SoftCommon.h"

namespace lyra::softgpu
{
    struct SoftMipLevel
    {
        uint  width       = 0;
        uint  height      = 0;
        uint  depth       = 0;
        usize row_pitch   = 0;
        usize layer_pitch = 0;
        usize byte_offset = 0;
        usize byte_size   = 0;
    };

    struct SoftTexture
    {
        GPUExtent3D          extent{1, 1, 1};
        GPUIntegerCoordinate mip_level_count = 1;
        GPUIntegerCoordinate array_layers    = 1;
        GPUTextureDimension  dimension       = GPUTextureDimension::x2D;
        GPUTextureFormat     format          = GPUTextureFormat::RGBA8UNORM;
        GPUTextureUsageFlags usage           = 0;
        CString              label           = "";

        Vector<ubyte>        data;
        Vector<SoftMipLevel> mips;

        SoftTexture() = default;
        explicit SoftTexture(const GPUTextureDescriptor& desc);

        usize               pixel_bytes() const;
        const SoftMipLevel& get_mip(uint mip) const;
        ubyte*              get_texel_ptr(uint x, uint y, uint z = 0, uint mip = 0, uint layer = 0);
        const ubyte*        get_texel_ptr(uint x, uint y, uint z = 0, uint mip = 0, uint layer = 0) const;

        Vector4 read_rgba(uint x, uint y, uint z = 0, uint mip = 0, uint layer = 0) const;
        void    write_rgba(uint x, uint y, const Vector4& color, uint z = 0, uint mip = 0, uint layer = 0);

        float read_depth(uint x, uint y, uint mip = 0, uint layer = 0) const;
        void  write_depth(uint x, uint y, float depth, uint mip = 0, uint layer = 0);

        ubyte read_stencil(uint x, uint y, uint mip = 0, uint layer = 0) const;
        void  write_stencil(uint x, uint y, ubyte stencil, uint mip = 0, uint layer = 0);

        void clear_color(const GPUColor& color, uint mip = 0, uint layer = 0);
        void clear_depth_stencil(float depth, ubyte stencil, bool clear_depth = true, bool clear_stencil = true);

        void destroy();
    };

    struct SoftTextureView
    {
        GPUTextureHandle        texture;
        GPUTextureFormat        format            = GPUTextureFormat::RGBA8UNORM;
        GPUTextureViewDimension dimension         = GPUTextureViewDimension::x2D;
        GPUTextureAspectFlags   aspect            = GPUTextureAspect::ALL;
        GPUIntegerCoordinate    base_mip_level    = 0;
        GPUIntegerCoordinate    mip_level_count   = 1;
        GPUIntegerCoordinate    base_array_layer  = 0;
        GPUIntegerCoordinate    array_layer_count = 1;
        CString                 label             = "";

        SoftTextureView() = default;
        SoftTextureView(GPUTextureHandle texture, const GPUTextureViewDescriptor& desc, const SoftTexture& tex);

        void destroy() {}
    };

    struct SlangTextureDimensions
    {
        uint shape             = 2;
        uint width             = 0;
        uint height            = 0;
        uint depth             = 1;
        uint numberOfLevels    = 1;
        uint arrayElementCount = 0;
    };

    struct ISlangHostTexture
    {
        virtual SlangTextureDimensions GetDimensions(int mipLevel = -1)                                                              = 0;
        virtual void                   Load(const int32_t* v, void* outData, usize dataSize)                                         = 0;
        virtual void                   Sample(void* samplerState, const float* loc, void* outData, usize dataSize)                   = 0;
        virtual void                   SampleLevel(void* samplerState, const float* loc, float level, void* outData, usize dataSize) = 0;
    };

    struct SoftHostTextureWrapper : public ISlangHostTexture
    {
        const SoftTexture*     tex  = nullptr;
        const SoftTextureView* view = nullptr;

        SoftHostTextureWrapper() = default;
        SoftHostTextureWrapper(const SoftTexture* t, const SoftTextureView* v) : tex(t), view(v) {}

        SlangTextureDimensions GetDimensions(int mipLevel = -1) override;
        void                   Load(const int32_t* v, void* outData, usize dataSize) override;
        void                   Sample(void* samplerState, const float* loc, void* outData, usize dataSize) override;
        void                   SampleLevel(void* samplerState, const float* loc, float level, void* outData, usize dataSize) override;
    };
} // namespace lyra::softgpu

#endif // LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTTEXTURE_H
