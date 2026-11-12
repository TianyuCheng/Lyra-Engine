#pragma once

#ifndef LYRA_ENGINE_RENDERING_SYSTEM_TEXTURES_H
#define LYRA_ENGINE_RENDERING_SYSTEM_TEXTURES_H

#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Compiler/SLCTypes.h>
#include <Lyra/Rendering/Geometry.h>
#include <Lyra/Assets/AMSServer.h>

namespace lyra
{
    /**
     * @brief centralized repository of built-in engine textures, fallback resources,
     * and procedural GPU-computed lookup tables (e.g. split-sum BRDF LUT, Rayleigh skymap).
     * Industry standard nomenclature: Unreal Engine (FSystemTextures), Frostbite (SystemTextures).
     */
    class SystemTextures
    {
    public:
        explicit SystemTextures();
        ~SystemTextures();

        SystemTextures(const SystemTextures&)            = delete;
        SystemTextures& operator=(const SystemTextures&) = delete;

        SystemTextures(SystemTextures&&) noexcept            = default;
        SystemTextures& operator=(SystemTextures&&) noexcept = default;

        /**
         * @brief initialize default GPU fallback textures and compute procedural textures via GPU compute shaders.
         */
        bool init(GPUDevice device, Compiler compiler = {}, AssetServer* ams = nullptr);

        /**
         * @brief waits for GPU device idle and safely releases all system textures and views.
         */
        void shutdown();

        // standard fallback textures
        GPUTextureHandle     get_white_texture() const { return white_texture; }
        GPUTextureViewHandle get_white_view() const { return white_view; }

        GPUTextureHandle     get_black_texture() const { return black_texture; }
        GPUTextureViewHandle get_black_view() const { return black_view; }

        // normal & bump / height map fallbacks
        GPUTextureHandle     get_normal_texture() const { return normal_texture; }
        GPUTextureViewHandle get_normal_view() const { return normal_view; }
        GPUTextureHandle     get_default_normal_texture() const { return normal_texture; }
        GPUTextureViewHandle get_default_normal_view() const { return normal_view; }

        GPUTextureHandle     get_bump_texture() const { return bump_texture; }
        GPUTextureViewHandle get_bump_view() const { return bump_view; }
        GPUTextureHandle     get_default_bump_texture() const { return bump_texture; }
        GPUTextureViewHandle get_default_bump_view() const { return bump_view; }

        GPUTextureHandle     get_default_pbr_texture() const { return default_pbr_texture; }
        GPUTextureViewHandle get_default_pbr_view() const { return default_pbr_view; }

        // magenta / purple for unsupported or missing materials
        GPUTextureHandle     get_magenta_texture() const { return magenta_texture; }
        GPUTextureViewHandle get_magenta_view() const { return magenta_view; }
        GPUTextureHandle     get_error_texture() const { return magenta_texture; }
        GPUTextureViewHandle get_error_view() const { return magenta_view; }

        // low-contrast subtle checkerboard for placeholder textures
        GPUTextureHandle     get_checkerboard_texture() const { return checkerboard_texture; }
        GPUTextureViewHandle get_checkerboard_view() const { return checkerboard_view; }

        // procedural rayleigh-scattering atmospheric skymap (pure sky gradient without sun disc or clouds)
        GPUTextureHandle     get_skymap_texture() const { return skymap_texture; }
        GPUTextureViewHandle get_skymap_view() const { return skymap_view; }

        // precomputed environment BRDF split-sum LUT
        GPUTextureHandle     get_brdf_lut_texture() const { return brdf_lut_texture; }
        GPUTextureViewHandle get_brdf_lut_view() const { return brdf_lut_view; }

        // precomputed IES photometric light profiles atlas (256x16)
        GPUTextureHandle     get_ies_atlas_texture() const { return ies_atlas_texture; }
        GPUTextureViewHandle get_ies_atlas_view() const { return ies_atlas_view; }

        // blue noise for dithering & ray tracing
        GPUTextureHandle     get_blue_noise_texture() const { return blue_noise_texture; }
        GPUTextureViewHandle get_blue_noise_view() const { return blue_noise_view; }

    private:
        void init_solid_textures(GPUDevice device);
        void init_procedural_textures(GPUDevice device, Compiler compiler, AssetServer* ams);

        bool dispatch_compute_lut(
            GPUDevice      device,
            Compiler       compiler,
            CString        module_name,
            CString        shader_path,
            GPUTexture     texture,
            GPUTextureView view,
            uint           width,
            uint           height,
            uint           group_size_x,
            uint           group_size_y);

    private:
        GPUDevice device;

        GPUTexture     white_texture;
        GPUTextureView white_view;

        GPUTexture     black_texture;
        GPUTextureView black_view;

        GPUTexture     normal_texture;
        GPUTextureView normal_view;

        GPUTexture     bump_texture;
        GPUTextureView bump_view;

        GPUTexture     default_pbr_texture;
        GPUTextureView default_pbr_view;

        GPUTexture     magenta_texture;
        GPUTextureView magenta_view;

        GPUTexture     checkerboard_texture;
        GPUTextureView checkerboard_view;

        GPUTexture     skymap_texture;
        GPUTextureView skymap_view;

        GPUTexture     brdf_lut_texture;
        GPUTextureView brdf_lut_view;

        GPUTexture     ies_atlas_texture;
        GPUTextureView ies_atlas_view;

        GPUTexture     blue_noise_texture;
        GPUTextureView blue_noise_view;

        bool initialized = false;
    };

} // namespace lyra

#endif // LYRA_ENGINE_RENDERING_SYSTEM_TEXTURES_H
