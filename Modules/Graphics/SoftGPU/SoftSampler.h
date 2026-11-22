#pragma once

#ifndef LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTSAMPLER_H
#define LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTSAMPLER_H

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

        float   wrap_coord(float coord, GPUAddressMode mode) const;
        Vector4 sample_2d(const SoftTexture& tex, const SoftTextureView& view, float u, float v, float lod = 0.0f) const;

        void destroy() {}
    };
} // namespace lyra::softgpu

#endif // LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTSAMPLER_H
