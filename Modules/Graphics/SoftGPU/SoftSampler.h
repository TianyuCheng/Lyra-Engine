#pragma once

#ifndef LYRA_MODULES_GRAPHICS_SOFTWARE_SOFTSAMPLER_H
#define LYRA_MODULES_GRAPHICS_SOFTWARE_SOFTSAMPLER_H

#include "SoftCommon.h"
#include "SoftTexture.h"

namespace lyra::softgpu
{
    struct SoftSampler
    {
        GPUSamplerDescriptor desc;
        CString              label = "";

        SoftSampler() = default;
        explicit SoftSampler(const GPUSamplerDescriptor& descriptor);

        auto wrap_coord(float coord, GPUAddressMode mode) const -> float;
        auto sample_2d(const SoftTexture& tex, const SoftTextureView& view, float u, float v, float lod = 0.0f) const -> Vector4;
    };
} // namespace lyra::softgpu

#endif // LYRA_MODULES_GRAPHICS_SOFTWARE_SOFTSAMPLER_H
