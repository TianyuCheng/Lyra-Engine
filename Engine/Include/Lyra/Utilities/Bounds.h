#pragma once

#ifndef LYRA_ENGINE_UTILITIES_BOUNDS_H
#define LYRA_ENGINE_UTILITIES_BOUNDS_H

#include <Lyra/Utilities/Math.h>

namespace lyra
{
    // =========================================================================
    // 2D Bounds (Rect)
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

        constexpr bool overlaps(const Bounds2D& b) const
        {
            return min.x <= b.max.x && max.x >= b.min.x && min.y <= b.max.y && max.y >= b.min.y;
        }
    };

    using Rect = Bounds2D;

    // =========================================================================
    // 3D Bounds (Box)
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

        constexpr bool overlaps(const Bounds3D& b) const
        {
            return min.x <= b.max.x && max.x >= b.min.x &&
                   min.y <= b.max.y && max.y >= b.min.y &&
                   min.z <= b.max.z && max.z >= b.min.z;
        }
    };

    using Box = Bounds3D;

} // namespace lyra

#endif // LYRA_ENGINE_UTILITIES_BOUNDS_H
