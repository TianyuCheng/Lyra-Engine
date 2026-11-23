#include "SoftTexture.h"
#include "SoftSampler.h"
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
        case GPUTextureFormat::RG8UINT:
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
        case GPUTextureFormat::RG11B10UFLOAT:
        case GPUTextureFormat::DEPTH32FLOAT:
        case GPUTextureFormat::DEPTH24PLUS_STENCIL8:
            return 4;
        case GPUTextureFormat::RGBA16FLOAT:
        case GPUTextureFormat::RGBA16UNORM:
        case GPUTextureFormat::RGBA16SNORM:
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

    uint w = std::max(1u, extent.width);
    uint h = std::max(1u, extent.height);
    uint d = std::max(1u, extent.depth);

    for (uint mip = 0; mip < mip_level_count; ++mip) {
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

const SoftMipLevel& SoftTexture::get_mip(uint mip) const
{
    uint idx = std::min(mip, mip_level_count - 1);
    return mips[idx];
}

ubyte* SoftTexture::get_texel_ptr(uint x, uint y, uint z, uint mip, uint layer)
{
    if (mip >= mip_level_count || layer >= array_layers) return nullptr;
    const auto& m = mips[mip];
    if (x >= m.width || y >= m.height || z >= m.depth) return nullptr;

    usize bpp    = pixel_bytes();
    usize offset = m.byte_offset + layer * m.layer_pitch + z * (m.row_pitch * m.height) + y * m.row_pitch + x * bpp;
    if (offset >= data.size()) return nullptr;
    return data.data() + offset;
}

const ubyte* SoftTexture::get_texel_ptr(uint x, uint y, uint z, uint mip, uint layer) const
{
    if (mip >= mip_level_count || layer >= array_layers) return nullptr;
    const auto& m = mips[mip];
    if (x >= m.width || y >= m.height || z >= m.depth) return nullptr;

    usize bpp    = pixel_bytes();
    usize offset = m.byte_offset + layer * m.layer_pitch + z * (m.row_pitch * m.height) + y * m.row_pitch + x * bpp;
    if (offset >= data.size()) return nullptr;
    return data.data() + offset;
}

Vector4 SoftTexture::read_rgba(uint x, uint y, uint z, uint mip, uint layer) const
{
    const ubyte* ptr = get_texel_ptr(x, y, z, mip, layer);
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
            const ushort* h = reinterpret_cast<const ushort*>(ptr);
            return Vector4(
                glm::unpackHalf1x16(h[0]),
                glm::unpackHalf1x16(h[1]),
                glm::unpackHalf1x16(h[2]),
                glm::unpackHalf1x16(h[3]));
        }
        case GPUTextureFormat::RGBA16UNORM:
        {
            const ushort* u = reinterpret_cast<const ushort*>(ptr);
            return Vector4(
                u[0] / 65535.0f,
                u[1] / 65535.0f,
                u[2] / 65535.0f,
                u[3] / 65535.0f);
        }
        case GPUTextureFormat::RGBA16SNORM:
        {
            const short* s = reinterpret_cast<const short*>(ptr);
            return Vector4(
                std::clamp(s[0] / 32767.0f, -1.0f, 1.0f),
                std::clamp(s[1] / 32767.0f, -1.0f, 1.0f),
                std::clamp(s[2] / 32767.0f, -1.0f, 1.0f),
                std::clamp(s[3] / 32767.0f, -1.0f, 1.0f));
        }
        case GPUTextureFormat::RG11B10UFLOAT:
        {
            uint      packed = *reinterpret_cast<const uint*>(ptr);
            glm::vec3 rgb    = glm::unpackF2x11_1x10(packed);
            return Vector4(rgb.r, rgb.g, rgb.b, 1.0f);
        }
        case GPUTextureFormat::R8UINT:
        {
            return Vector4(static_cast<float>(ptr[0]), 0.0f, 0.0f, 1.0f);
        }
        case GPUTextureFormat::RG8UINT:
        {
            return Vector4(static_cast<float>(ptr[0]), static_cast<float>(ptr[1]), 0.0f, 1.0f);
        }
        case GPUTextureFormat::RGBA32UINT:
        {
            const uint* u = reinterpret_cast<const uint*>(ptr);
            return Vector4(
                static_cast<float>(u[0]),
                static_cast<float>(u[1]),
                static_cast<float>(u[2]),
                static_cast<float>(u[3]));
        }
        case GPUTextureFormat::R32UINT:
        {
            const uint* u = reinterpret_cast<const uint*>(ptr);
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
            uint  val = *reinterpret_cast<const uint*>(ptr);
            float d   = static_cast<float>(val & 0x00FFFFFF) / 16777215.0f;
            return Vector4(d, d, d, 1.0f);
        }
        case GPUTextureFormat::DEPTH16UNORM:
        {
            ushort d16 = *reinterpret_cast<const ushort*>(ptr);
            float  d   = static_cast<float>(d16) / 65535.0f;
            return Vector4(d, d, d, 1.0f);
        }
        default:
            return Vector4(ptr[0] / 255.0f, ptr[1] / 255.0f, ptr[2] / 255.0f, ptr[3] / 255.0f);
    }
}

void SoftTexture::write_rgba(uint x, uint y, const Vector4& color, uint z, uint mip, uint layer)
{
    ubyte* ptr = get_texel_ptr(x, y, z, mip, layer);
    if (!ptr) return;

    auto clamp01 = [](float v) -> float {
        return std::clamp(v, 0.0f, 1.0f);
    };

    switch (format) {
        case GPUTextureFormat::RGBA8UNORM:
        case GPUTextureFormat::RGBA8UNORM_SRGB:
            ptr[0] = static_cast<ubyte>(clamp01(color.r) * 255.0f + 0.5f);
            ptr[1] = static_cast<ubyte>(clamp01(color.g) * 255.0f + 0.5f);
            ptr[2] = static_cast<ubyte>(clamp01(color.b) * 255.0f + 0.5f);
            ptr[3] = static_cast<ubyte>(clamp01(color.a) * 255.0f + 0.5f);
            break;
        case GPUTextureFormat::BGRA8UNORM:
        case GPUTextureFormat::BGRA8UNORM_SRGB:
            ptr[0] = static_cast<ubyte>(clamp01(color.b) * 255.0f + 0.5f);
            ptr[1] = static_cast<ubyte>(clamp01(color.g) * 255.0f + 0.5f);
            ptr[2] = static_cast<ubyte>(clamp01(color.r) * 255.0f + 0.5f);
            ptr[3] = static_cast<ubyte>(clamp01(color.a) * 255.0f + 0.5f);
            break;
        case GPUTextureFormat::R8UNORM:
            ptr[0] = static_cast<ubyte>(clamp01(color.r) * 255.0f + 0.5f);
            break;
        case GPUTextureFormat::RG8UNORM:
            ptr[0] = static_cast<ubyte>(clamp01(color.r) * 255.0f + 0.5f);
            ptr[1] = static_cast<ubyte>(clamp01(color.g) * 255.0f + 0.5f);
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
            ushort* h = reinterpret_cast<ushort*>(ptr);
            h[0]      = glm::packHalf1x16(color.r);
            h[1]      = glm::packHalf1x16(color.g);
            h[2]      = glm::packHalf1x16(color.b);
            h[3]      = glm::packHalf1x16(color.a);
            break;
        }
        case GPUTextureFormat::RGBA16UNORM:
        {
            ushort* u = reinterpret_cast<ushort*>(ptr);
            u[0]      = static_cast<ushort>(clamp01(color.r) * 65535.0f + 0.5f);
            u[1]      = static_cast<ushort>(clamp01(color.g) * 65535.0f + 0.5f);
            u[2]      = static_cast<ushort>(clamp01(color.b) * 65535.0f + 0.5f);
            u[3]      = static_cast<ushort>(clamp01(color.a) * 65535.0f + 0.5f);
            break;
        }
        case GPUTextureFormat::RGBA16SNORM:
        {
            short* s           = reinterpret_cast<short*>(ptr);
            auto   clamp_snorm = [](float v) -> short {
                return static_cast<short>(std::clamp(v, -1.0f, 1.0f) * 32767.0f + (v >= 0.0f ? 0.5f : -0.5f));
            };
            s[0] = clamp_snorm(color.r);
            s[1] = clamp_snorm(color.g);
            s[2] = clamp_snorm(color.b);
            s[3] = clamp_snorm(color.a);
            break;
        }
        case GPUTextureFormat::RG11B10UFLOAT:
        {
            uint* u = reinterpret_cast<uint*>(ptr);
            *u      = glm::packF2x11_1x10(glm::vec3(std::max(0.0f, color.r), std::max(0.0f, color.g), std::max(0.0f, color.b)));
            break;
        }
        case GPUTextureFormat::R8UINT:
        {
            ptr[0] = static_cast<ubyte>(std::clamp(color.r, 0.0f, 255.0f) + 0.5f);
            break;
        }
        case GPUTextureFormat::RG8UINT:
        {
            ptr[0] = static_cast<ubyte>(std::clamp(color.r, 0.0f, 255.0f) + 0.5f);
            ptr[1] = static_cast<ubyte>(std::clamp(color.g, 0.0f, 255.0f) + 0.5f);
            break;
        }
        case GPUTextureFormat::RGBA32UINT:
        {
            uint* u = reinterpret_cast<uint*>(ptr);
            u[0]    = static_cast<uint>(color.r);
            u[1]    = static_cast<uint>(color.g);
            u[2]    = static_cast<uint>(color.b);
            u[3]    = static_cast<uint>(color.a);
            break;
        }
        case GPUTextureFormat::R32UINT:
        {
            uint* u = reinterpret_cast<uint*>(ptr);
            u[0]    = static_cast<uint>(color.r);
            break;
        }
        default:
            ptr[0] = static_cast<ubyte>(clamp01(color.r) * 255.0f + 0.5f);
            ptr[1] = static_cast<ubyte>(clamp01(color.g) * 255.0f + 0.5f);
            ptr[2] = static_cast<ubyte>(clamp01(color.b) * 255.0f + 0.5f);
            ptr[3] = static_cast<ubyte>(clamp01(color.a) * 255.0f + 0.5f);
            break;
    }
}

float SoftTexture::read_depth(uint x, uint y, uint mip, uint layer) const
{
    const ubyte* ptr = get_texel_ptr(x, y, 0, mip, layer);
    if (!ptr) return 1.0f;

    if (format == GPUTextureFormat::DEPTH32FLOAT || format == GPUTextureFormat::DEPTH32FLOAT_STENCIL8) {
        return *reinterpret_cast<const float*>(ptr);
    } else if (format == GPUTextureFormat::DEPTH24PLUS_STENCIL8) {
        uint val = *reinterpret_cast<const uint*>(ptr);
        uint d24 = val & 0x00FFFFFF;
        return static_cast<float>(d24) / 16777215.0f;
    } else if (format == GPUTextureFormat::DEPTH16UNORM) {
        ushort d16 = *reinterpret_cast<const ushort*>(ptr);
        return static_cast<float>(d16) / 65535.0f;
    }
    return 1.0f;
}

void SoftTexture::write_depth(uint x, uint y, float depth, uint mip, uint layer)
{
    ubyte* ptr = get_texel_ptr(x, y, 0, mip, layer);
    if (!ptr) return;

    if (format == GPUTextureFormat::DEPTH32FLOAT || format == GPUTextureFormat::DEPTH32FLOAT_STENCIL8) {
        *reinterpret_cast<float*>(ptr) = depth;
    } else if (format == GPUTextureFormat::DEPTH24PLUS_STENCIL8) {
        uint* p   = reinterpret_cast<uint*>(ptr);
        uint  d24 = static_cast<uint>(std::clamp(depth, 0.0f, 1.0f) * 16777215.0f);
        *p        = (*p & 0xFF000000) | (d24 & 0x00FFFFFF);
    } else if (format == GPUTextureFormat::DEPTH16UNORM) {
        *reinterpret_cast<ushort*>(ptr) = static_cast<ushort>(std::clamp(depth, 0.0f, 1.0f) * 65535.0f);
    }
}

ubyte SoftTexture::read_stencil(uint x, uint y, uint mip, uint layer) const
{
    const ubyte* ptr = get_texel_ptr(x, y, 0, mip, layer);
    if (!ptr) return 0;

    if (format == GPUTextureFormat::DEPTH24PLUS_STENCIL8) {
        return ptr[3];
    } else if (format == GPUTextureFormat::DEPTH32FLOAT_STENCIL8) {
        return ptr[4];
    }
    return 0;
}

void SoftTexture::write_stencil(uint x, uint y, ubyte stencil, uint mip, uint layer)
{
    ubyte* ptr = get_texel_ptr(x, y, 0, mip, layer);
    if (!ptr) return;

    if (format == GPUTextureFormat::DEPTH24PLUS_STENCIL8) {
        ptr[3] = stencil;
    } else if (format == GPUTextureFormat::DEPTH32FLOAT_STENCIL8) {
        ptr[4] = stencil;
    }
}

void SoftTexture::clear_color(const GPUColor& color, uint mip, uint layer)
{
    if (mip >= mip_level_count || layer >= array_layers) return;
    const auto& m = mips[mip];
    Vector4     c(static_cast<float>(color.r), static_cast<float>(color.g), static_cast<float>(color.b), static_cast<float>(color.a));

    for (uint y = 0; y < m.height; ++y) {
        for (uint x = 0; x < m.width; ++x) {
            write_rgba(x, y, c, 0, mip, layer);
        }
    }
}

void SoftTexture::clear_depth_stencil(float depth, ubyte stencil, bool clear_d, bool clear_s)
{
    for (uint mip = 0; mip < mip_level_count; ++mip) {
        const auto& m = mips[mip];
        for (uint layer = 0; layer < array_layers; ++layer) {
            for (uint y = 0; y < m.height; ++y) {
                for (uint x = 0; x < m.width; ++x) {
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

SlangTextureDimensions SoftHostTextureWrapper::GetDimensions(int mipLevel)
{
    if (!tex) return {2, 0, 0, 0, 0, 0};
    uint        m   = (mipLevel >= 0) ? std::min(static_cast<uint>(mipLevel), tex->mip_level_count - 1) : 0;
    const auto& mip = (m < tex->mips.size()) ? tex->mips[m] : SoftMipLevel{tex->extent.width, tex->extent.height, tex->extent.depth};
    return {2, mip.width, mip.height, mip.depth, tex->mip_level_count, 0};
}

void SoftHostTextureWrapper::Load(const int32_t* v, void* outData, usize dataSize)
{
    if (!tex || !outData || !v) return;
    int32_t x   = v[0];
    int32_t y   = v[1];
    int32_t mip = (v[2] >= 0) ? v[2] : 0;
    if (x < 0 || y < 0) return;

    if (tex->format == GPUTextureFormat::DEPTH32FLOAT ||
        tex->format == GPUTextureFormat::DEPTH24PLUS_STENCIL8 ||
        tex->format == GPUTextureFormat::DEPTH16UNORM) {
        float d = tex->read_depth(static_cast<uint>(x), static_cast<uint>(y), mip);
        if (dataSize == sizeof(float)) {
            *reinterpret_cast<float*>(outData) = d;
        } else if (dataSize >= sizeof(Vector4)) {
            *reinterpret_cast<Vector4*>(outData) = Vector4(d, d, d, 1.0f);
        }
    } else if (tex->format == GPUTextureFormat::RGBA32UINT) {
        const ubyte* ptr = tex->get_texel_ptr(static_cast<uint>(x), static_cast<uint>(y), 0, mip);
        if (ptr) {
            std::memcpy(outData, ptr, std::min(dataSize, sizeof(uint) * 4));
        } else {
            std::memset(outData, 0, dataSize);
        }
    } else if (tex->format == GPUTextureFormat::R8UINT) {
        const ubyte* ptr = tex->get_texel_ptr(static_cast<uint>(x), static_cast<uint>(y), 0, mip);
        uint         val = ptr ? static_cast<uint>(*ptr) : 0u;
        if (dataSize == sizeof(uint)) {
            *reinterpret_cast<uint*>(outData) = val;
        } else if (dataSize >= sizeof(Vector4)) {
            *reinterpret_cast<Vector4*>(outData) = Vector4(static_cast<float>(val), 0.0f, 0.0f, 1.0f);
        } else {
            std::memcpy(outData, &val, std::min(dataSize, sizeof(uint)));
        }
    } else if (tex->format == GPUTextureFormat::RG8UINT) {
        const ubyte* ptr     = tex->get_texel_ptr(static_cast<uint>(x), static_cast<uint>(y), 0, mip);
        uint         vals[2] = {ptr ? static_cast<uint>(ptr[0]) : 0u, ptr ? static_cast<uint>(ptr[1]) : 0u};
        if (dataSize == sizeof(uint) * 2) {
            std::memcpy(outData, vals, sizeof(uint) * 2);
        } else if (dataSize == sizeof(uint)) {
            *reinterpret_cast<uint*>(outData) = vals[0];
        } else if (dataSize >= sizeof(Vector4)) {
            *reinterpret_cast<Vector4*>(outData) = Vector4(static_cast<float>(vals[0]), static_cast<float>(vals[1]), 0.0f, 1.0f);
        } else {
            std::memcpy(outData, vals, std::min(dataSize, sizeof(vals)));
        }
    } else {
        Vector4 c = tex->read_rgba(static_cast<uint>(x), static_cast<uint>(y), 0, mip);
        if (dataSize == sizeof(float)) {
            *reinterpret_cast<float*>(outData) = c.r;
        } else if (dataSize >= sizeof(Vector4)) {
            *reinterpret_cast<Vector4*>(outData) = c;
        } else {
            std::memcpy(outData, &c, dataSize);
        }
    }
}

void SoftHostTextureWrapper::Sample(void* samplerState, const float* loc, void* outData, usize dataSize)
{
    if (!tex || !outData || !loc) return;
    float                  u = loc[0];
    float                  v = loc[1];
    const SoftSampler*     s = reinterpret_cast<const SoftSampler*>(samplerState);
    SoftTextureView        default_view;
    const SoftTextureView* vptr = view ? view : &default_view;
    Vector4                c;
    if (s) {
        c = s->sample_2d(*tex, *vptr, u, v, 0.0f);
    } else {
        uint px = static_cast<uint>(std::clamp(u * tex->extent.width, 0.0f, float(tex->extent.width > 0 ? tex->extent.width - 1 : 0)));
        uint py = static_cast<uint>(std::clamp(v * tex->extent.height, 0.0f, float(tex->extent.height > 0 ? tex->extent.height - 1 : 0)));
        c       = tex->read_rgba(px, py);
    }

    if (dataSize == sizeof(float)) {
        *reinterpret_cast<float*>(outData) = c.r;
    } else if (dataSize >= sizeof(Vector4)) {
        *reinterpret_cast<Vector4*>(outData) = c;
    } else {
        std::memcpy(outData, &c, dataSize);
    }
}

void SoftHostTextureWrapper::SampleLevel(void* samplerState, const float* loc, float level, void* outData, usize dataSize)
{
    if (!tex || !outData || !loc) return;
    float                  u = loc[0];
    float                  v = loc[1];
    const SoftSampler*     s = reinterpret_cast<const SoftSampler*>(samplerState);
    SoftTextureView        default_view;
    const SoftTextureView* vptr = view ? view : &default_view;
    Vector4                c;
    if (s) {
        c = s->sample_2d(*tex, *vptr, u, v, level);
    } else {
        uint px = static_cast<uint>(std::clamp(u * tex->extent.width, 0.0f, float(tex->extent.width > 0 ? tex->extent.width - 1 : 0)));
        uint py = static_cast<uint>(std::clamp(v * tex->extent.height, 0.0f, float(tex->extent.height > 0 ? tex->extent.height - 1 : 0)));
        c       = tex->read_rgba(px, py, 0, static_cast<uint>(std::max(0.0f, level)));
    }

    if (dataSize == sizeof(float)) {
        *reinterpret_cast<float*>(outData) = c.r;
    } else if (dataSize >= sizeof(Vector4)) {
        *reinterpret_cast<Vector4*>(outData) = c;
    } else {
        std::memcpy(outData, &c, dataSize);
    }
}
