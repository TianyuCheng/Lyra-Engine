#pragma once

#ifndef LYRA_ENGINE_RENDERING_MATERIAL_PARAMS_H
#define LYRA_ENGINE_RENDERING_MATERIAL_PARAMS_H

#include <cstddef>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Math.h>

namespace lyra
{
    /**
     * @brief bitflags for MaterialParams flags field.
     */
    namespace MaterialFlags
    {
        inline constexpr uint NONE             = 0;
        inline constexpr uint DOUBLE_SIDED     = 1u << 0;
        inline constexpr uint ALPHA_TEST       = 1u << 1;
        inline constexpr uint UNLIT            = 1u << 2;
        inline constexpr uint HAS_NORMAL_MAP   = 1u << 3;
        inline constexpr uint HAS_EMISSIVE_MAP = 1u << 4;
    } // namespace MaterialFlags

    /**
     * @brief 16-byte aligned unified material parameters layout adhering to std140 / std430.
     *
     * Total size: exactly 80 bytes. Aligned to 16 bytes.
     */
    struct alignas(16) MaterialParams
    {
        Vector4 base_color           = Vector4(1.0f); // 16 bytes (offset 0)
        float   metallic             = 0.0f;          // 4 bytes  (offset 16)
        float   roughness            = 0.5f;          // 4 bytes  (offset 20)
        float   clear_coat           = 0.0f;          // 4 bytes  (offset 24)
        float   clear_coat_roughness = 0.04f;         // 4 bytes  (offset 28)
        Vector4 emissive             = Vector4(0.0f); // 16 bytes (offset 32)
        Vector4 custom_data          = Vector4(0.0f); // 16 bytes (offset 48) - SSS radius / cloth sheen / NPR
        uint    shading_model_id     = 0;             // 4 bytes  (offset 64)
        float   alpha_cutoff         = 0.5f;          // 4 bytes  (offset 68)
        uint    flags                = 0;             // 4 bytes  (offset 72)
        uint    custom_params_offset = 0;             // 4 bytes  (offset 76)
    };

    static_assert(sizeof(MaterialParams) == 80, "MaterialParams must be exactly 80 bytes");
    static_assert(alignof(MaterialParams) == 16, "MaterialParams must be 16-byte aligned");
    static_assert(offsetof(MaterialParams, base_color) == 0);
    static_assert(offsetof(MaterialParams, metallic) == 16);
    static_assert(offsetof(MaterialParams, roughness) == 20);
    static_assert(offsetof(MaterialParams, clear_coat) == 24);
    static_assert(offsetof(MaterialParams, clear_coat_roughness) == 28);
    static_assert(offsetof(MaterialParams, emissive) == 32);
    static_assert(offsetof(MaterialParams, custom_data) == 48);
    static_assert(offsetof(MaterialParams, shading_model_id) == 64);
    static_assert(offsetof(MaterialParams, alpha_cutoff) == 68);
    static_assert(offsetof(MaterialParams, flags) == 72);
    static_assert(offsetof(MaterialParams, custom_params_offset) == 76);

} // namespace lyra

#endif // LYRA_ENGINE_RENDERING_MATERIAL_PARAMS_H
