#pragma once

#ifndef LYRA_MODULES_GRAPHICS_SOFTWARE_SOFTTEXTURE_H
#define LYRA_MODULES_GRAPHICS_SOFTWARE_SOFTTEXTURE_H

#include "SoftCommon.h"

namespace lyra::softgpu
{
    struct SoftMipLevel
    {
        uint32_t width       = 0;
        uint32_t height      = 0;
        uint32_t depth       = 0;
        usize    row_pitch   = 0;
        usize    layer_pitch = 0;
        usize    byte_offset = 0;
        usize    byte_size   = 0;
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

        Vector<uint8_t>      data;
        Vector<SoftMipLevel> mips;

        SoftTexture() = default;
        explicit SoftTexture(const GPUTextureDescriptor& desc);

        auto pixel_bytes() const -> usize;
        auto get_mip(uint32_t mip) const -> const SoftMipLevel&;
        auto get_texel_ptr(uint32_t x, uint32_t y, uint32_t z = 0, uint32_t mip = 0, uint32_t layer = 0) -> uint8_t*;
        auto get_texel_ptr(uint32_t x, uint32_t y, uint32_t z = 0, uint32_t mip = 0, uint32_t layer = 0) const -> const uint8_t*;

        auto read_rgba(uint32_t x, uint32_t y, uint32_t z = 0, uint32_t mip = 0, uint32_t layer = 0) const -> Vector4;
        auto write_rgba(uint32_t x, uint32_t y, const Vector4& color, uint32_t z = 0, uint32_t mip = 0, uint32_t layer = 0) -> void;

        auto read_depth(uint32_t x, uint32_t y, uint32_t mip = 0, uint32_t layer = 0) const -> float;
        auto write_depth(uint32_t x, uint32_t y, float depth, uint32_t mip = 0, uint32_t layer = 0) -> void;

        auto read_stencil(uint32_t x, uint32_t y, uint32_t mip = 0, uint32_t layer = 0) const -> uint8_t;
        auto write_stencil(uint32_t x, uint32_t y, uint8_t stencil, uint32_t mip = 0, uint32_t layer = 0) -> void;

        auto clear_color(const GPUColor& color, uint32_t mip = 0, uint32_t layer = 0) -> void;
        auto clear_depth_stencil(float depth, uint8_t stencil, bool clear_depth = true, bool clear_stencil = true) -> void;

        auto destroy() -> void;
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
    };
} // namespace lyra::softgpu

#endif // LYRA_MODULES_GRAPHICS_SOFTWARE_SOFTTEXTURE_H
