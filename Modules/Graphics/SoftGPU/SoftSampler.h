#pragma once

#ifndef LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTSAMPLER_H
#define LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTSAMPLER_H

#include "SoftCommon.h"
#include "SoftTexture.h"

namespace lyra::softgpu
{
    struct SoftPixelGradients
    {
        float dudx          = 0.0f;
        float dvdx          = 0.0f;
        float dudy          = 0.0f;
        float dvdy          = 0.0f;
        int   px            = 0;
        int   py            = 0;
        bool  has_gradients = false;
    };

    struct SoftSamplerTLS
    {
        static inline thread_local SoftPixelGradients current_gradients{};
    };

    struct SoftSampler
    {
        GPUSamplerDescriptor desc;
        CString              label = "";

        SoftSampler() = default;
        explicit SoftSampler(const GPUSamplerDescriptor& descriptor);

        float   wrap_coord(float coord, GPUAddressMode mode) const;
        Vector4 sample_2d(const SoftTexture& tex, const SoftTextureView& view, float u, float v, float lod = 0.0f) const;
        Vector4 sample_2d_grad(const SoftTexture& tex, const SoftTextureView& view, float u, float v,
            float dudx, float dvdx, float dudy, float dvdy) const;

        void destroy() {}
    };
} // namespace lyra::softgpu

#endif // LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTSAMPLER_H
