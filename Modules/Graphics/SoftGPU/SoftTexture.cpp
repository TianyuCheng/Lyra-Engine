#include "SoftTexture.h"
#include <glm/gtc/packing.hpp>

using namespace lyra;
using namespace lyra::softgpu;

static usize get_bytes_per_pixel(GPUTextureFormat format)
{
    switch (format) {
        case GPUTextureFormat::R8UNORM:
        case GPUTextureFormat::R8UINT:
            return 1;
        case GPUTextureFormat::RG8UNORM:
        case GPUTextureFormat::R16UINT:
        case GPUTextureFormat::DEPTH16UNORM:
            return 2;
        case GPUTextureFormat::RGBA8UNORM:
        case GPUTextureFormat::RGBA8UNORM_SRGB:
        case GPUTextureFormat::BGRA8UNORM:
        case GPUTextureFormat::BGRA8UNORM_SRGB:
        case GPUTextureFormat::R32FLOAT:
        case GPUTextureFormat::R32UINT:
        case GPUTextureFormat::RG16FLOAT:
        case GPUTextureFormat::DEPTH32FLOAT:
        case GPUTextureFormat::DEPTH24PLUS_STENCIL8:
            return 4;
        case GPUTextureFormat::RGBA16FLOAT:
        case GPUTextureFormat::RG32FLOAT:
        case GPUTextureFormat::DEPTH32FLOAT_STENCIL8:
            return 8;
        case GPUTextureFormat::RGBA32FLOAT:
        case GPUTextureFormat::RGBA32UINT:
            return 16;
        default:
            return 4;
    }
}

SoftTexture::SoftTexture(const GPUTextureDescriptor& desc)
    : extent(desc.size),
      mip_level_count(std::max(1u, desc.mip_level_count)),
      array_layers(std::max(1u, desc.array_layers)),
      dimension(desc.dimension),
      format(desc.format),
      usage(desc.usage),
      label(desc.label)
{
    usize bpp = get_bytes_per_pixel(format);
    mips.resize(mip_level_count);

    usize current_offset = 0;

    uint32_t w = std::max(1u, extent.width);
    uint32_t h = std::max(1u, extent.height);
    uint32_t d = std::max(1u, extent.depth);

    for (uint32_t mip = 0; mip < mip_level_count; ++mip) {
        auto& m       = mips[mip];
        m.width       = std::max(1u, w >> mip);
        m.height      = std::max(1u, h >> mip);
        m.depth       = std::max(1u, d >> mip);
        m.row_pitch   = static_cast<usize>(m.width) * bpp;
        m.layer_pitch = m.row_pitch * m.height * m.depth;
        m.byte_offset = current_offset;
        m.byte_size   = m.layer_pitch * array_layers;

        current_offset += m.byte_size;
    }

    data.resize(current_offset, 0);
}

usize SoftTexture::pixel_bytes() const
{
    return get_bytes_per_pixel(format);
}

const SoftMipLevel& SoftTexture::get_mip(uint32_t mip) const
{
    uint32_t idx = std::min(mip, mip_level_count - 1);
    return mips[idx];
}

uint8_t* SoftTexture::get_texel_ptr(uint32_t x, uint32_t y, uint32_t z, uint32_t mip, uint32_t layer)
{
    if (mip >= mip_level_count || layer >= array_layers) return nullptr;
    const auto& m = mips[mip];
    if (x >= m.width || y >= m.height || z >= m.depth) return nullptr;

    usize bpp    = pixel_bytes();
    usize offset = m.byte_offset + layer * m.layer_pitch + z * (m.row_pitch * m.height) + y * m.row_pitch + x * bpp;
    if (offset >= data.size()) return nullptr;
    return data.data() + offset;
}

const uint8_t* SoftTexture::get_texel_ptr(uint32_t x, uint32_t y, uint32_t z, uint32_t mip, uint32_t layer) const
{
    if (mip >= mip_level_count || layer >= array_layers) return nullptr;
    const auto& m = mips[mip];
    if (x >= m.width || y >= m.height || z >= m.depth) return nullptr;

    usize bpp    = pixel_bytes();
    usize offset = m.byte_offset + layer * m.layer_pitch + z * (m.row_pitch * m.height) + y * m.row_pitch + x * bpp;
    if (offset >= data.size()) return nullptr;
    return data.data() + offset;
}

Vector4 SoftTexture::read_rgba(uint32_t x, uint32_t y, uint32_t z, uint32_t mip, uint32_t layer) const
{
    const uint8_t* ptr = get_texel_ptr(x, y, z, mip, layer);
    if (!ptr) return Vector4(0.0f);

    switch (format) {
        case GPUTextureFormat::RGBA8UNORM:
        case GPUTextureFormat::RGBA8UNORM_SRGB:
            return Vector4(ptr[0] / 255.0f, ptr[1] / 255.0f, ptr[2] / 255.0f, ptr[3] / 255.0f);
        case GPUTextureFormat::BGRA8UNORM:
        case GPUTextureFormat::BGRA8UNORM_SRGB:
            return Vector4(ptr[2] / 255.0f, ptr[1] / 255.0f, ptr[0] / 255.0f, ptr[3] / 255.0f);
        case GPUTextureFormat::R8UNORM:
            return Vector4(ptr[0] / 255.0f, 0.0f, 0.0f, 1.0f);
        case GPUTextureFormat::RG8UNORM:
            return Vector4(ptr[0] / 255.0f, ptr[1] / 255.0f, 0.0f, 1.0f);
        case GPUTextureFormat::RGBA32FLOAT:
        {
            const float* f = reinterpret_cast<const float*>(ptr);
            return Vector4(f[0], f[1], f[2], f[3]);
        }
        case GPUTextureFormat::R32FLOAT:
        {
            const float* f = reinterpret_cast<const float*>(ptr);
            return Vector4(f[0], 0.0f, 0.0f, 1.0f);
        }
        case GPUTextureFormat::RG32FLOAT:
        {
            const float* f = reinterpret_cast<const float*>(ptr);
            return Vector4(f[0], f[1], 0.0f, 1.0f);
        }
        case GPUTextureFormat::RGBA16FLOAT:
        {
            const uint16_t* h = reinterpret_cast<const uint16_t*>(ptr);
            return Vector4(
                glm::unpackHalf1x16(h[0]),
                glm::unpackHalf1x16(h[1]),
                glm::unpackHalf1x16(h[2]),
                glm::unpackHalf1x16(h[3])
            );
        }
        case GPUTextureFormat::RGBA32UINT:
        {
            const uint32_t* u = reinterpret_cast<const uint32_t*>(ptr);
            return Vector4(
                static_cast<float>(u[0]),
                static_cast<float>(u[1]),
                static_cast<float>(u[2]),
                static_cast<float>(u[3])
            );
        }
        case GPUTextureFormat::R32UINT:
        {
            const uint32_t* u = reinterpret_cast<const uint32_t*>(ptr);
            return Vector4(static_cast<float>(u[0]), 0.0f, 0.0f, 1.0f);
        }
        case GPUTextureFormat::DEPTH32FLOAT:
        case GPUTextureFormat::DEPTH32FLOAT_STENCIL8:
        {
            float d = *reinterpret_cast<const float*>(ptr);
            return Vector4(d, d, d, 1.0f);
        }
        case GPUTextureFormat::DEPTH24PLUS_STENCIL8:
        {
            uint32_t val = *reinterpret_cast<const uint32_t*>(ptr);
            float d = static_cast<float>(val & 0x00FFFFFF) / 16777215.0f;
            return Vector4(d, d, d, 1.0f);
        }
        case GPUTextureFormat::DEPTH16UNORM:
        {
            uint16_t d16 = *reinterpret_cast<const uint16_t*>(ptr);
            float d = static_cast<float>(d16) / 65535.0f;
            return Vector4(d, d, d, 1.0f);
        }
        default:
            return Vector4(ptr[0] / 255.0f, ptr[1] / 255.0f, ptr[2] / 255.0f, ptr[3] / 255.0f);
    }
}

void SoftTexture::write_rgba(uint32_t x, uint32_t y, const Vector4& color, uint32_t z, uint32_t mip, uint32_t layer)
{
    uint8_t* ptr = get_texel_ptr(x, y, z, mip, layer);
    if (!ptr) return;

    auto clamp01 = [](float v) -> float {
        return std::clamp(v, 0.0f, 1.0f);
    };

    switch (format) {
        case GPUTextureFormat::RGBA8UNORM:
        case GPUTextureFormat::RGBA8UNORM_SRGB:
            ptr[0] = static_cast<uint8_t>(clamp01(color.r) * 255.0f + 0.5f);
            ptr[1] = static_cast<uint8_t>(clamp01(color.g) * 255.0f + 0.5f);
            ptr[2] = static_cast<uint8_t>(clamp01(color.b) * 255.0f + 0.5f);
            ptr[3] = static_cast<uint8_t>(clamp01(color.a) * 255.0f + 0.5f);
            break;
        case GPUTextureFormat::BGRA8UNORM:
        case GPUTextureFormat::BGRA8UNORM_SRGB:
            ptr[0] = static_cast<uint8_t>(clamp01(color.b) * 255.0f + 0.5f);
            ptr[1] = static_cast<uint8_t>(clamp01(color.g) * 255.0f + 0.5f);
            ptr[2] = static_cast<uint8_t>(clamp01(color.r) * 255.0f + 0.5f);
            ptr[3] = static_cast<uint8_t>(clamp01(color.a) * 255.0f + 0.5f);
            break;
        case GPUTextureFormat::R8UNORM:
            ptr[0] = static_cast<uint8_t>(clamp01(color.r) * 255.0f + 0.5f);
            break;
        case GPUTextureFormat::RG8UNORM:
            ptr[0] = static_cast<uint8_t>(clamp01(color.r) * 255.0f + 0.5f);
            ptr[1] = static_cast<uint8_t>(clamp01(color.g) * 255.0f + 0.5f);
            break;
        case GPUTextureFormat::RGBA32FLOAT:
        {
            float* f = reinterpret_cast<float*>(ptr);
            f[0]     = color.r;
            f[1]     = color.g;
            f[2]     = color.b;
            f[3]     = color.a;
            break;
        }
        case GPUTextureFormat::R32FLOAT:
        {
            float* f = reinterpret_cast<float*>(ptr);
            f[0]     = color.r;
            break;
        }
        case GPUTextureFormat::RG32FLOAT:
        {
            float* f = reinterpret_cast<float*>(ptr);
            f[0]     = color.r;
            f[1]     = color.g;
            break;
        }
        case GPUTextureFormat::RGBA16FLOAT:
        {
            uint16_t* h = reinterpret_cast<uint16_t*>(ptr);
            h[0]        = glm::packHalf1x16(color.r);
            h[1]        = glm::packHalf1x16(color.g);
            h[2]        = glm::packHalf1x16(color.b);
            h[3]        = glm::packHalf1x16(color.a);
            break;
        }
        case GPUTextureFormat::RGBA32UINT:
        {
            uint32_t* u = reinterpret_cast<uint32_t*>(ptr);
            u[0]        = static_cast<uint32_t>(color.r);
            u[1]        = static_cast<uint32_t>(color.g);
            u[2]        = static_cast<uint32_t>(color.b);
            u[3]        = static_cast<uint32_t>(color.a);
            break;
        }
        case GPUTextureFormat::R32UINT:
        {
            uint32_t* u = reinterpret_cast<uint32_t*>(ptr);
            u[0]        = static_cast<uint32_t>(color.r);
            break;
        }
        default:
            ptr[0] = static_cast<uint8_t>(clamp01(color.r) * 255.0f + 0.5f);
            ptr[1] = static_cast<uint8_t>(clamp01(color.g) * 255.0f + 0.5f);
            ptr[2] = static_cast<uint8_t>(clamp01(color.b) * 255.0f + 0.5f);
            ptr[3] = static_cast<uint8_t>(clamp01(color.a) * 255.0f + 0.5f);
            break;
    }
}

float SoftTexture::read_depth(uint32_t x, uint32_t y, uint32_t mip, uint32_t layer) const
{
    const uint8_t* ptr = get_texel_ptr(x, y, 0, mip, layer);
    if (!ptr) return 1.0f;

    if (format == GPUTextureFormat::DEPTH32FLOAT || format == GPUTextureFormat::DEPTH32FLOAT_STENCIL8) {
        return *reinterpret_cast<const float*>(ptr);
    } else if (format == GPUTextureFormat::DEPTH24PLUS_STENCIL8) {
        uint32_t val = *reinterpret_cast<const uint32_t*>(ptr);
        uint32_t d24 = val & 0x00FFFFFF;
        return static_cast<float>(d24) / 16777215.0f;
    } else if (format == GPUTextureFormat::DEPTH16UNORM) {
        uint16_t d16 = *reinterpret_cast<const uint16_t*>(ptr);
        return static_cast<float>(d16) / 65535.0f;
    }
    return 1.0f;
}

void SoftTexture::write_depth(uint32_t x, uint32_t y, float depth, uint32_t mip, uint32_t layer)
{
    uint8_t* ptr = get_texel_ptr(x, y, 0, mip, layer);
    if (!ptr) return;

    if (format == GPUTextureFormat::DEPTH32FLOAT || format == GPUTextureFormat::DEPTH32FLOAT_STENCIL8) {
        *reinterpret_cast<float*>(ptr) = depth;
    } else if (format == GPUTextureFormat::DEPTH24PLUS_STENCIL8) {
        uint32_t* p   = reinterpret_cast<uint32_t*>(ptr);
        uint32_t  d24 = static_cast<uint32_t>(std::clamp(depth, 0.0f, 1.0f) * 16777215.0f);
        *p            = (*p & 0xFF000000) | (d24 & 0x00FFFFFF);
    } else if (format == GPUTextureFormat::DEPTH16UNORM) {
        *reinterpret_cast<uint16_t*>(ptr) = static_cast<uint16_t>(std::clamp(depth, 0.0f, 1.0f) * 65535.0f);
    }
}

uint8_t SoftTexture::read_stencil(uint32_t x, uint32_t y, uint32_t mip, uint32_t layer) const
{
    const uint8_t* ptr = get_texel_ptr(x, y, 0, mip, layer);
    if (!ptr) return 0;

    if (format == GPUTextureFormat::DEPTH24PLUS_STENCIL8) {
        return ptr[3];
    } else if (format == GPUTextureFormat::DEPTH32FLOAT_STENCIL8) {
        return ptr[4];
    }
    return 0;
}

void SoftTexture::write_stencil(uint32_t x, uint32_t y, uint8_t stencil, uint32_t mip, uint32_t layer)
{
    uint8_t* ptr = get_texel_ptr(x, y, 0, mip, layer);
    if (!ptr) return;

    if (format == GPUTextureFormat::DEPTH24PLUS_STENCIL8) {
        ptr[3] = stencil;
    } else if (format == GPUTextureFormat::DEPTH32FLOAT_STENCIL8) {
        ptr[4] = stencil;
    }
}

void SoftTexture::clear_color(const GPUColor& color, uint32_t mip, uint32_t layer)
{
    if (mip >= mip_level_count || layer >= array_layers) return;
    const auto& m = mips[mip];
    Vector4     c(static_cast<float>(color.r), static_cast<float>(color.g), static_cast<float>(color.b), static_cast<float>(color.a));

    for (uint32_t y = 0; y < m.height; ++y) {
        for (uint32_t x = 0; x < m.width; ++x) {
            write_rgba(x, y, c, 0, mip, layer);
        }
    }
}

void SoftTexture::clear_depth_stencil(float depth, uint8_t stencil, bool clear_d, bool clear_s)
{
    for (uint32_t mip = 0; mip < mip_level_count; ++mip) {
        const auto& m = mips[mip];
        for (uint32_t layer = 0; layer < array_layers; ++layer) {
            for (uint32_t y = 0; y < m.height; ++y) {
                for (uint32_t x = 0; x < m.width; ++x) {
                    if (clear_d) write_depth(x, y, depth, mip, layer);
                    if (clear_s) write_stencil(x, y, stencil, mip, layer);
                }
            }
        }
    }
}

void SoftTexture::destroy()
{
    data.clear();
    data.shrink_to_fit();
    mips.clear();
}

SoftTextureView::SoftTextureView(GPUTextureHandle tex, const GPUTextureViewDescriptor& desc, const SoftTexture& parent)
    : texture(tex),
      format(desc.format),
      dimension(desc.dimension),
      aspect(desc.aspect),
      base_mip_level(desc.base_mip_level),
      mip_level_count(std::max(1u, desc.mip_level_count)),
      base_array_layer(desc.base_array_layer),
      array_layer_count(std::max(1u, desc.array_layer_count)),
      label(desc.label)
{
}
