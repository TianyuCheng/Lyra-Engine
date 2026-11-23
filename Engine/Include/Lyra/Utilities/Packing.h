#pragma once

#ifndef LYRA_ENGINE_UTILITIES_PACKING_H
#define LYRA_ENGINE_UTILITIES_PACKING_H

#include <cmath>
#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Stdint.h>

namespace lyra
{
    // =========================================================================
    // Sign helpers
    // =========================================================================

    /**
     * @brief returns +1.0f if v >= 0.0f, else -1.0f.
     */
    inline float sign_not_zero(float v)
    {
        return v >= 0.0f ? 1.0f : -1.0f;
    }

    /**
     * @brief component-wise sign_not_zero for 2d vectors.
     */
    inline Vector2 sign_not_zero(Vector2 v)
    {
        return {sign_not_zero(v.x), sign_not_zero(v.y)};
    }

    // =========================================================================
    // Octahedral Normal Encoding & Decoding
    // Reference: Meyer et al. 2010, Cigolle et al. 2014
    // =========================================================================

    /**
     * @brief encodes a normalized 3d unit normal vector into 2d octahedral coordinates in [-1, 1].
     * @param n normalized 3d unit normal vector.
     * @return encoded 2d coordinate in [-1, 1].
     */
    inline Vector2 octahedral_encode(Vector3 n)
    {
        float   l1 = std::abs(n.x) + std::abs(n.y) + std::abs(n.z);
        Vector2 p  = Vector2(n.x, n.y) * (1.0f / (l1 > 1e-6f ? l1 : 1e-6f));
        if (n.z < 0.0f) {
            p = (Vector2(1.0f) - glm::abs(Vector2(p.y, p.x))) * sign_not_zero(p);
        }
        return p;
    }

    /**
     * @brief decodes 2d octahedral coordinates in [-1, 1] back into a normalized 3d unit normal vector.
     * @param e 2d octahedral coordinate in [-1, 1].
     * @return reconstructed normalized 3d unit normal vector.
     */
    inline Vector3 octahedral_decode(Vector2 e)
    {
        Vector3 v(e.x, e.y, 1.0f - std::abs(e.x) - std::abs(e.y));
        if (v.z < 0.0f) {
            Vector2 xy = (Vector2(1.0f) - glm::abs(Vector2(v.y, v.x))) * sign_not_zero(Vector2(v.x, v.y));
            v.x        = xy.x;
            v.y        = xy.y;
        }
        return glm::normalize(v);
    }

    /**
     * @brief alias for octahedral_encode matching shader conventions.
     */
    inline Vector2 oct_encode(Vector3 n)
    {
        return octahedral_encode(n);
    }

    /**
     * @brief alias for octahedral_decode matching shader conventions.
     */
    inline Vector3 oct_decode(Vector2 e)
    {
        return octahedral_decode(e);
    }

    // =========================================================================
    // UNORM & SNORM 8-bit / 16-bit Packing
    // =========================================================================

    /**
     * @brief packs four normalized floats in [0, 1] into a 32-bit uint (8 bits per channel).
     */
    inline uint pack_unorm_4x8(Vector4 v)
    {
        auto u  = glm::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f;
        auto ux = static_cast<uint>(u.x);
        auto uy = static_cast<uint>(u.y);
        auto uz = static_cast<uint>(u.z);
        auto uw = static_cast<uint>(u.w);
        return (ux & 0xFFu) | ((uy & 0xFFu) << 8) | ((uz & 0xFFu) << 16) | ((uw & 0xFFu) << 24);
    }

    /**
     * @brief unpacks four normalized floats in [0, 1] from a 32-bit uint.
     */
    inline Vector4 unpack_unorm_4x8(uint packed)
    {
        return {
            static_cast<float>(packed & 0xFFu) / 255.0f,
            static_cast<float>((packed >> 8) & 0xFFu) / 255.0f,
            static_cast<float>((packed >> 16) & 0xFFu) / 255.0f,
            static_cast<float>((packed >> 24) & 0xFFu) / 255.0f,
        };
    }

    /**
     * @brief packs four signed normalized floats in [-1, 1] into a 32-bit uint (8 bits per channel).
     */
    inline uint pack_snorm_4x8(Vector4 v)
    {
        auto i = glm::clamp(v, -1.0f, 1.0f) * 127.0f + Vector4(v.x >= 0.0f ? 0.5f : -0.5f,
                                                           v.y >= 0.0f ? 0.5f : -0.5f,
                                                           v.z >= 0.0f ? 0.5f : -0.5f,
                                                           v.w >= 0.0f ? 0.5f : -0.5f);
        auto ix = static_cast<int32_t>(i.x);
        auto iy = static_cast<int32_t>(i.y);
        auto iz = static_cast<int32_t>(i.z);
        auto iw = static_cast<int32_t>(i.w);
        return (static_cast<uint>(ix) & 0xFFu) |
               ((static_cast<uint>(iy) & 0xFFu) << 8) |
               ((static_cast<uint>(iz) & 0xFFu) << 16) |
               ((static_cast<uint>(iw) & 0xFFu) << 24);
    }

    /**
     * @brief unpacks four signed normalized floats in [-1, 1] from a 32-bit uint.
     */
    inline Vector4 unpack_snorm_4x8(uint packed)
    {
        auto ix = static_cast<int8_t>(packed & 0xFFu);
        auto iy = static_cast<int8_t>((packed >> 8) & 0xFFu);
        auto iz = static_cast<int8_t>((packed >> 16) & 0xFFu);
        auto iw = static_cast<int8_t>((packed >> 24) & 0xFFu);
        return glm::clamp(Vector4(static_cast<float>(ix) / 127.0f,
                              static_cast<float>(iy) / 127.0f,
                              static_cast<float>(iz) / 127.0f,
                              static_cast<float>(iw) / 127.0f),
            -1.0f, 1.0f);
    }

    /**
     * @brief packs two normalized floats in [0, 1] into a 32-bit uint (16 bits per channel).
     */
    inline uint pack_unorm_2x16(Vector2 v)
    {
        auto u  = glm::clamp(v, 0.0f, 1.0f) * 65535.0f + 0.5f;
        auto ux = static_cast<uint>(u.x);
        auto uy = static_cast<uint>(u.y);
        return (ux & 0xFFFFu) | ((uy & 0xFFFFu) << 16);
    }

    /**
     * @brief unpacks two normalized floats in [0, 1] from a 32-bit uint.
     */
    inline Vector2 unpack_unorm_2x16(uint packed)
    {
        return {
            static_cast<float>(packed & 0xFFFFu) / 65535.0f,
            static_cast<float>((packed >> 16) & 0xFFFFu) / 65535.0f,
        };
    }

    /**
     * @brief packs two signed normalized floats in [-1, 1] into a 32-bit uint (16 bits per channel).
     */
    inline uint pack_snorm_2x16(Vector2 v)
    {
        int32_t x = static_cast<int32_t>(glm::clamp(v.x, -1.0f, 1.0f) * 32767.0f + (v.x >= 0.0f ? 0.5f : -0.5f));
        int32_t y = static_cast<int32_t>(glm::clamp(v.y, -1.0f, 1.0f) * 32767.0f + (v.y >= 0.0f ? 0.5f : -0.5f));
        return (static_cast<uint>(x) & 0xFFFFu) | ((static_cast<uint>(y) & 0xFFFFu) << 16);
    }

    /**
     * @brief unpacks two signed normalized floats in [-1, 1] from a 32-bit uint.
     */
    inline Vector2 unpack_snorm_2x16(uint packed)
    {
        int16_t x = static_cast<int16_t>(packed & 0xFFFFu);
        int16_t y = static_cast<int16_t>((packed >> 16) & 0xFFFFu);
        return glm::clamp(Vector2(static_cast<float>(x) / 32767.0f, static_cast<float>(y) / 32767.0f), -1.0f, 1.0f);
    }

} // namespace lyra

#endif // LYRA_ENGINE_UTILITIES_PACKING_H
