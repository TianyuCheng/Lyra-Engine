#pragma once

#ifndef LYRA_ENGINE_UTILITIES_GEOMETRY_H
#define LYRA_ENGINE_UTILITIES_GEOMETRY_H

#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Utilities/Enums.h>

namespace lyra
{
    // =========================================================================
    // 2D Radial: Circle
    // =========================================================================

    struct Circle
    {
        Vector2 center = {0.0f, 0.0f};
        float   radius = 1.0f;

        constexpr Circle() = default;
        constexpr Circle(Vector2 center, float radius) : center(center), radius(radius) {}

        inline bool contains(Vector2 p) const
        {
            Vector2 diff = p - center;
            return glm::dot(diff, diff) <= (radius * radius);
        }
    };

    using BoundingCircle = Circle;

    // =========================================================================
    // 3D Radial: Sphere
    // =========================================================================

    struct Sphere
    {
        Vector3 center = {0.0f, 0.0f, 0.0f};
        float   radius = 1.0f;

        constexpr Sphere() = default;
        constexpr Sphere(Vector3 center, float radius) : center(center), radius(radius) {}

        inline bool contains(Vector3 p) const
        {
            Vector3 diff = p - center;
            return glm::dot(diff, diff) <= (radius * radius);
        }
    };

    using BoundingSphere = Sphere;

    // =========================================================================
    // Linear: Ray2D & Ray3D
    // =========================================================================

    struct Ray2D
    {
        Vector2 origin    = {0.0f, 0.0f};
        Vector2 direction = {1.0f, 0.0f};

        constexpr Ray2D() = default;
        constexpr Ray2D(Vector2 origin, Vector2 direction) : origin(origin), direction(direction) {}

        constexpr Vector2 at(float t) const { return origin + direction * t; }
    };

    struct Ray3D
    {
        Vector3 origin    = {0.0f, 0.0f, 0.0f};
        Vector3 direction = {0.0f, 0.0f, 1.0f};

        constexpr Ray3D() = default;
        constexpr Ray3D(Vector3 origin, Vector3 direction) : origin(origin), direction(direction) {}

        constexpr Vector3 at(float t) const { return origin + direction * t; }
    };

    // =========================================================================
    // Planar: Plane & Frustum
    // =========================================================================

    struct Plane
    {
        Vector3 normal   = {0.0f, 1.0f, 0.0f};
        float   distance = 0.0f; // plane equation: dot(normal, p) + distance = 0

        constexpr Plane() = default;
        constexpr Plane(Vector3 normal, float distance) : normal(normal), distance(distance) {}
        inline Plane(const Vector4& v) : normal(v.x, v.y, v.z), distance(v.w)
        {
            normalize();
        }

        inline void normalize()
        {
            float len = glm::length(normal);
            if (len > 1e-6f) {
                normal /= len;
                distance /= len;
            }
        }

        constexpr float signed_distance(Vector3 p) const
        {
            return (normal.x * p.x + normal.y * p.y + normal.z * p.z) + distance;
        }
    };

    struct Frustum
    {
        enum PlaneIndex : uint
        {
            LEFT = 0,
            RIGHT,
            BOTTOM,
            TOP,
            NEAR_PLANE,
            FAR_PLANE
        };

        Array<Plane, enum_count_v<PlaneIndex>> planes;

        /**
         * @brief extract 6 view frustum clipping planes from a view-projection matrix.
         *
         * assumes depth range [0, 1] (standard in Lyra/Vulkan/D3D).
         */
        static inline Frustum from_view_projection(const Matrix4x4& vp)
        {
            Frustum f;

            // in glm, vp[col][row] - extract rows
            Vector4 row0 = {vp[0][0], vp[1][0], vp[2][0], vp[3][0]};
            Vector4 row1 = {vp[0][1], vp[1][1], vp[2][1], vp[3][1]};
            Vector4 row2 = {vp[0][2], vp[1][2], vp[2][2], vp[3][2]};
            Vector4 row3 = {vp[0][3], vp[1][3], vp[2][3], vp[3][3]};

            f.planes[LEFT]       = Plane(row3 + row0);
            f.planes[RIGHT]      = Plane(row3 - row0);
            f.planes[BOTTOM]     = Plane(row3 + row1);
            f.planes[TOP]        = Plane(row3 - row1);
            f.planes[NEAR_PLANE] = Plane(row2);        // for depth in [0, 1], near plane is row2
            f.planes[FAR_PLANE]  = Plane(row3 - row2); // far plane is row3 - row2

            return f;
        }

        inline bool contains(Vector3 point) const
        {
            for (const auto& plane : planes) {
                if (plane.signed_distance(point) < 0.0f) {
                    return false;
                }
            }
            return true;
        }
    };

} // namespace lyra

#endif // LYRA_ENGINE_UTILITIES_GEOMETRY_H
