#pragma once

#ifndef LYRA_ENGINE_UTILITIES_BOUNDS_H
#define LYRA_ENGINE_UTILITIES_BOUNDS_H

#include <cmath>
#include <algorithm>
#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Stdint.h>

namespace lyra
{
    // =========================================================================
    // 2D Bounds (Axis-Aligned Bounding Box / Rect)
    // =========================================================================

    struct Bounds2D
    {
        Vector2 min = {0.0f, 0.0f};
        Vector2 max = {0.0f, 0.0f};

        constexpr Bounds2D() = default;
        constexpr Bounds2D(Vector2 min, Vector2 max) : min(min), max(max) {}
        constexpr Bounds2D(float min_x, float min_y, float max_x, float max_y) : min(min_x, min_y), max(max_x, max_y) {}

        constexpr float   width() const { return max.x - min.x; }
        constexpr float   height() const { return max.y - min.y; }
        constexpr Vector2 size() const { return {max.x - min.x, max.y - min.y}; }
        constexpr Vector2 center() const { return (min + max) * 0.5f; }
        constexpr Vector2 extents() const { return (max - min) * 0.5f; }

        constexpr bool contains(Vector2 p) const
        {
            return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y;
        }

        constexpr bool contains(const Bounds2D& b) const
        {
            return b.min.x >= min.x && b.max.x <= max.x &&
                   b.min.y >= min.y && b.max.y <= max.y;
        }

        constexpr bool overlaps(const Bounds2D& b) const
        {
            return min.x <= b.max.x && max.x >= b.min.x &&
                   min.y <= b.max.y && max.y >= b.min.y;
        }

        constexpr void expand(Vector2 p)
        {
            min.x = std::min(min.x, p.x);
            min.y = std::min(min.y, p.y);
            max.x = std::max(max.x, p.x);
            max.y = std::max(max.y, p.y);
        }

        constexpr void expand(const Bounds2D& b)
        {
            min.x = std::min(min.x, b.min.x);
            min.y = std::min(min.y, b.min.y);
            max.x = std::max(max.x, b.max.x);
            max.y = std::max(max.y, b.max.y);
        }

        constexpr Bounds2D merged(const Bounds2D& b) const
        {
            return Bounds2D(
                std::min(min.x, b.min.x),
                std::min(min.y, b.min.y),
                std::max(max.x, b.max.x),
                std::max(max.y, b.max.y));
        }
    };

    using Rect          = Bounds2D;
    using BoundingBox2D = Bounds2D;

    // =========================================================================
    // 3D Bounds (Axis-Aligned Bounding Box / Box)
    // =========================================================================

    struct Bounds3D
    {
        Vector3 min = {0.0f, 0.0f, 0.0f};
        Vector3 max = {0.0f, 0.0f, 0.0f};

        constexpr Bounds3D() = default;
        constexpr Bounds3D(Vector3 min, Vector3 max) : min(min), max(max) {}
        constexpr Bounds3D(float min_x, float min_y, float min_z, float max_x, float max_y, float max_z)
            : min(min_x, min_y, min_z), max(max_x, max_y, max_z)
        {
        }

        constexpr float   width() const { return max.x - min.x; }
        constexpr float   height() const { return max.y - min.y; }
        constexpr float   depth() const { return max.z - min.z; }
        constexpr Vector3 size() const { return {max.x - min.x, max.y - min.y, max.z - min.z}; }
        constexpr Vector3 center() const { return (min + max) * 0.5f; }
        constexpr Vector3 extents() const { return (max - min) * 0.5f; }

        constexpr bool contains(Vector3 p) const
        {
            return p.x >= min.x && p.x <= max.x &&
                   p.y >= min.y && p.y <= max.y &&
                   p.z >= min.z && p.z <= max.z;
        }

        constexpr bool contains(const Bounds3D& b) const
        {
            return b.min.x >= min.x && b.max.x <= max.x &&
                   b.min.y >= min.y && b.max.y <= max.y &&
                   b.min.z >= min.z && b.max.z <= max.z;
        }

        constexpr bool overlaps(const Bounds3D& b) const
        {
            return min.x <= b.max.x && max.x >= b.min.x &&
                   min.y <= b.max.y && max.y >= b.min.y &&
                   min.z <= b.max.z && max.z >= b.min.z;
        }

        constexpr void expand(Vector3 p)
        {
            min.x = std::min(min.x, p.x);
            min.y = std::min(min.y, p.y);
            min.z = std::min(min.z, p.z);
            max.x = std::max(max.x, p.x);
            max.y = std::max(max.y, p.y);
            max.z = std::max(max.z, p.z);
        }

        constexpr void expand(const Bounds3D& b)
        {
            min.x = std::min(min.x, b.min.x);
            min.y = std::min(min.y, b.min.y);
            min.z = std::min(min.z, b.min.z);
            max.x = std::max(max.x, b.max.x);
            max.y = std::max(max.y, b.max.y);
            max.z = std::max(max.z, b.max.z);
        }

        constexpr Bounds3D merged(const Bounds3D& b) const
        {
            return Bounds3D(
                std::min(min.x, b.min.x),
                std::min(min.y, b.min.y),
                std::min(min.z, b.min.z),
                std::max(max.x, b.max.x),
                std::max(max.y, b.max.y),
                std::max(max.z, b.max.z));
        }

        inline Bounds3D transformed(const Matrix4x4& m) const
        {
            Vector3 c          = center();
            Vector3 e          = extents();
            Vector3 new_center = Vector3(m * Vector4(c, 1.0f));
            Vector3 new_extents(
                std::abs(m[0][0]) * e.x + std::abs(m[1][0]) * e.y + std::abs(m[2][0]) * e.z,
                std::abs(m[0][1]) * e.x + std::abs(m[1][1]) * e.y + std::abs(m[2][1]) * e.z,
                std::abs(m[0][2]) * e.x + std::abs(m[1][2]) * e.y + std::abs(m[2][2]) * e.z);
            return Bounds3D(new_center - new_extents, new_center + new_extents);
        }
    };

    using Box         = Bounds3D;
    using BoundingBox = Bounds3D;

} // namespace lyra

#endif // LYRA_ENGINE_UTILITIES_BOUNDS_H
