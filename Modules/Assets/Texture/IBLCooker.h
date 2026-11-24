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
     * @return 6D array of face pixels: out_mip_faces[mip_level][face_index][pixel_index * 4]
     */
    void prefilter_environment_cubemap(
        const float*                   hdr_rgba,
        int                            src_width,
        int                            src_height,
        int                            base_face_size,
        int                            num_mips,
        Vector<Vector<Vector<float>>>& out_mip_faces);

    /**
     * @brief Saves a multi-mip 6-face cubemap to a KTX2 container on disk.
     */
    bool save_cubemap_to_ktx2(
        const Vector<Vector<Vector<float>>>& mip_faces,
        int                                  base_size,
        int                                  num_mips,
        const Path&                          target_path,
        Logger                               logger);

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
