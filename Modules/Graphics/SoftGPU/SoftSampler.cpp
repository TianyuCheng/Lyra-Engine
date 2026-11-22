#include "SoftSampler.h"

using namespace lyra;
using namespace lyra::softgpu;

SoftSampler::SoftSampler(const GPUSamplerDescriptor& descriptor)
    : desc(descriptor), label(descriptor.label)
{
}

float SoftSampler::wrap_coord(float coord, GPUAddressMode mode) const
{
    switch (mode) {
        case GPUAddressMode::CLAMP_TO_EDGE:
            return std::clamp(coord, 0.0f, 1.0f);
        case GPUAddressMode::REPEAT:
            return coord - std::floor(coord);
        case GPUAddressMode::MIRROR_REPEAT:
        {
            float t = std::fabs(coord);
            int   i = static_cast<int>(t);
            float f = t - i;
            return (i % 2 == 0) ? f : (1.0f - f);
        }
        default:
            return std::clamp(coord, 0.0f, 1.0f);
    }
}

Vector4 SoftSampler::sample_2d(const SoftTexture& tex, const SoftTextureView& view, float u, float v, float lod) const
{
    float u_wrapped = wrap_coord(u, desc.address_mode_u);
    float v_wrapped = wrap_coord(v, desc.address_mode_v);

    lod             = std::clamp(lod, desc.lod_min_clamp, desc.lod_max_clamp);
    uint  mip0      = static_cast<uint>(lod);
    uint  mip1      = mip0 + 1;
    float mip_fract = lod - mip0;

    auto sample_single_mip = [&](uint mip_idx) -> Vector4 {
        uint        effective_mip = std::clamp(view.base_mip_level + mip_idx, 0u, tex.mip_level_count - 1);
        const auto& m             = tex.get_mip(effective_mip);

        if (desc.mag_filter == GPUFilterMode::NEAREST) {
            int  x  = static_cast<int>(std::floor(u_wrapped * m.width));
            int  y  = static_cast<int>(std::floor(v_wrapped * m.height));
            uint cx = std::clamp(x, 0, static_cast<int>(m.width - 1));
            uint cy = std::clamp(y, 0, static_cast<int>(m.height - 1));
            return tex.read_rgba(cx, cy, 0, effective_mip, view.base_array_layer);
        } else {
            // bilinear
            float px = u_wrapped * m.width - 0.5f;
            float py = v_wrapped * m.height - 0.5f;

            int x0 = static_cast<int>(std::floor(px));
            int y0 = static_cast<int>(std::floor(py));
            int x1 = x0 + 1;
            int y1 = y0 + 1;

            float fx = px - x0;
            float fy = py - y0;

            auto wrap_index = [](int idx, int dim, GPUAddressMode mode) -> uint {
                if (mode == GPUAddressMode::REPEAT) {
                    idx = idx % dim;
                    if (idx < 0) idx += dim;
                    return static_cast<uint>(idx);
                } else if (mode == GPUAddressMode::MIRROR_REPEAT) {
                    if (idx < 0) idx = -idx - 1;
                    int m = idx % (2 * dim);
                    if (m >= dim) m = 2 * dim - 1 - m;
                    return static_cast<uint>(m);
                } else {
                    return static_cast<uint>(std::clamp(idx, 0, dim - 1));
                }
            };

            uint cx0 = wrap_index(x0, static_cast<int>(m.width), desc.address_mode_u);
            uint cx1 = wrap_index(x1, static_cast<int>(m.width), desc.address_mode_u);
            uint cy0 = wrap_index(y0, static_cast<int>(m.height), desc.address_mode_v);
            uint cy1 = wrap_index(y1, static_cast<int>(m.height), desc.address_mode_v);

            Vector4 c00 = tex.read_rgba(cx0, cy0, 0, effective_mip, view.base_array_layer);
            Vector4 c10 = tex.read_rgba(cx1, cy0, 0, effective_mip, view.base_array_layer);
            Vector4 c01 = tex.read_rgba(cx0, cy1, 0, effective_mip, view.base_array_layer);
            Vector4 c11 = tex.read_rgba(cx1, cy1, 0, effective_mip, view.base_array_layer);

            Vector4 c0 = glm::mix(c00, c10, fx);
            Vector4 c1 = glm::mix(c01, c11, fx);
            return glm::mix(c0, c1, fy);
        }
    };

    if (desc.mipmap_filter == GPUMipmapFilterMode::LINEAR && mip1 < view.mip_level_count) {
        Vector4 s0 = sample_single_mip(mip0);
        Vector4 s1 = sample_single_mip(mip1);
        return glm::mix(s0, s1, mip_fract);
    } else {
        return sample_single_mip(mip0);
    }
}
