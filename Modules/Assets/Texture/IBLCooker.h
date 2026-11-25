#pragma once

#ifndef LYRA_MODULES_ASSETS_TEXTURE_IBL_COOKER_H
#define LYRA_MODULES_ASSETS_TEXTURE_IBL_COOKER_H

#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Logger.h>
#include <Lyra/Utilities/Config.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Assets/AMSAPI.h>

namespace lyra::texture
{
    /**
     * @brief Result of environment map processing including cooked cubemap path and SH9 irradiance coefficients.
     */
    struct IBLCookResult
    {
        Path              cubemap_path;
        Array<Vector4, 9> sh9{}; // 9 RGB spherical harmonics coefficients for diffuse irradiance
        uint              base_resolution = 512;
        uint              mip_levels      = 6;
        bool              success         = false;
    };

    /**
     * @brief Flat contiguous storage for a multi-mip 6-face cubemap.
     * Replaces 3-level nested dynamic vectors to eliminate heap fragmentation.
     */
    struct CubemapMipChain
    {
        uint          base_size = 0;
        uint          num_mips  = 0;
        Vector<float> pixels;
        Vector<usize> mip_offsets;

        void init(uint base_face_size, uint mips)
        {
            base_size = base_face_size;
            num_mips  = mips;
            mip_offsets.resize(num_mips);

            usize total_floats = 0;
            for (uint mip = 0; mip < num_mips; ++mip) {
                mip_offsets[mip] = total_floats;
                uint face_size   = std::max(1u, base_size >> mip);
                total_floats += static_cast<usize>(face_size) * face_size * 4u * 6u;
            }
            pixels.resize(total_floats);
        }

        [[nodiscard]] auto get_face_slice(uint mip, uint face) const -> Span<float>
        {
            if (mip >= num_mips || face >= 6) return {};
            uint  face_size = std::max(1u, base_size >> mip);
            usize face_len  = static_cast<usize>(face_size) * face_size * 4u;
            usize offset    = mip_offsets[mip] + face * face_len;
            return Span<float>(const_cast<float*>(&pixels[offset]), face_len);
        }
    };

    /**
     * @brief Computes 9-coefficient RGB Spherical Harmonics from an equirectangular HDR float image,
     *        convolved with the clamped cosine lobe for direct diffuse irradiance evaluation.
     */
    void compute_sh9_from_equirectangular(
        const float*       hdr_rgba,
        int                width,
        int                height,
        Array<Vector4, 9>& out_sh);

    /**
     * @brief Convolves equirectangular HDR image into a 6-face cubemap mip chain (512 down to 16)
     *        using GGX importance sampling per perceptual roughness level.
     */
    void prefilter_environment_cubemap(
        const float*     hdr_rgba,
        int              src_width,
        int              src_height,
        int              base_face_size,
        int              num_mips,
        CubemapMipChain& out_cubemap);

    /**
     * @brief Saves a multi-mip 6-face cubemap to a KTX2 container on disk.
     */
    bool save_cubemap_to_ktx2(
        const CubemapMipChain& cubemap,
        const Path&            target_path,
        Logger                 logger);

    /**
     * @brief Complete end-to-end processing of an equirectangular HDR image into prefiltered IBL assets.
     */
    auto process_environment_map(
        JSON&        metadata,
        const float* hdr_rgba,
        int          width,
        int          height,
        OSPath       caches_root,
        Logger       logger) -> IBLCookResult;

} // namespace lyra::texture

#endif // LYRA_MODULES_ASSETS_TEXTURE_IBL_COOKER_H
