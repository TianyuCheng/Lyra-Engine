#pragma once

#ifndef LYRA_ENGINE_UTILITIES_INTERSECT_H
#define LYRA_ENGINE_UTILITIES_INTERSECT_H

#include <cmath>
#include <limits>
#include <algorithm>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Bounds.h>
#include <Lyra/Utilities/Geometry.h>

namespace lyra
{
    // =========================================================================
    // Containment & Hit Types
    // =========================================================================

    enum struct ContainmentType : uint
    {
        DISJOINT   = 0, ///< geometry is entirely outside
        INTERSECTS = 1, ///< geometry intersects the boundary
        CONTAINS   = 2  ///< geometry is entirely inside
    };

    struct RayHit2D
    {
        float   distance = 0.0f;
        Vector2 point    = {0.0f, 0.0f};

        bool operator<(const RayHit2D& other) const { return distance < other.distance; }
    };

    struct RayHit3D
    {
        float   distance = 0.0f;
        Vector3 point    = {0.0f, 0.0f, 0.0f};

        bool operator<(const RayHit3D& other) const { return distance < other.distance; }
    };

    // =========================================================================
    // Ray-Box Intersections (Slab Method)
    // Convention: intersects(Ray, Target, ...)
    // =========================================================================

    inline bool intersects(const Ray2D& ray, const Bounds2D& box, float& out_t_min, float& out_t_max)
    {
        float t_min = -std::numeric_limits<float>::infinity();
        float t_max = +std::numeric_limits<float>::infinity();

        for (int i = 0; i < 2; ++i) {
            float o    = (i == 0) ? ray.origin.x : ray.origin.y;
            float d    = (i == 0) ? ray.direction.x : ray.direction.y;
            float bmin = (i == 0) ? box.min.x : box.min.y;
            float bmax = (i == 0) ? box.max.x : box.max.y;

            if (std::abs(d) < 1e-8f) {
                if (o < bmin || o > bmax) return false;
            } else {
                float inv_d = 1.0f / d;
                float t0    = (bmin - o) * inv_d;
                float t1    = (bmax - o) * inv_d;
                if (t0 > t1) std::swap(t0, t1);
                t_min = std::max(t_min, t0);
                t_max = std::min(t_max, t1);
                if (t_min > t_max) return false;
            }
        }

        out_t_min = t_min;
        out_t_max = t_max;
        return t_max >= 0.0f;
    }

    inline bool intersects(const Ray2D& ray, const Bounds2D& box, float& out_t)
    {
        float t_min = 0.0f;
        float t_max = 0.0f;
        if (intersects(ray, box, t_min, t_max)) {
            out_t = (t_min >= 0.0f) ? t_min : 0.0f;
            return true;
        }
        return false;
    }

    inline bool intersects(const Ray2D& ray, const Bounds2D& box)
    {
        float t_min = 0.0f;
        float t_max = 0.0f;
        return intersects(ray, box, t_min, t_max);
    }

    inline bool intersects(const Ray3D& ray, const Bounds3D& box, float& out_t_min, float& out_t_max)
    {
        float t_min = -std::numeric_limits<float>::infinity();
        float t_max = +std::numeric_limits<float>::infinity();

        for (int i = 0; i < 3; ++i) {
            float o    = (i == 0) ? ray.origin.x : ((i == 1) ? ray.origin.y : ray.origin.z);
            float d    = (i == 0) ? ray.direction.x : ((i == 1) ? ray.direction.y : ray.direction.z);
            float bmin = (i == 0) ? box.min.x : ((i == 1) ? box.min.y : box.min.z);
            float bmax = (i == 0) ? box.max.x : ((i == 1) ? box.max.y : box.max.z);

            if (std::abs(d) < 1e-8f) {
                if (o < bmin || o > bmax) return false;
            } else {
                float inv_d = 1.0f / d;
                float t0    = (bmin - o) * inv_d;
                float t1    = (bmax - o) * inv_d;
                if (t0 > t1) std::swap(t0, t1);
                t_min = std::max(t_min, t0);
                t_max = std::min(t_max, t1);
                if (t_min > t_max) return false;
            }
        }

        out_t_min = t_min;
        out_t_max = t_max;
        return t_max >= 0.0f;
    }

    inline bool intersects(const Ray3D& ray, const Bounds3D& box, float& out_t)
    {
        float t_min = 0.0f;
        float t_max = 0.0f;
        if (intersects(ray, box, t_min, t_max)) {
            out_t = (t_min >= 0.0f) ? t_min : 0.0f;
            return true;
        }
        return false;
    }

    inline bool intersects(const Ray3D& ray, const Bounds3D& box)
    {
        float t_min = 0.0f;
        float t_max = 0.0f;
        return intersects(ray, box, t_min, t_max);
    }

    // =========================================================================
    // Radial Overlaps: Circle-Box & Sphere-Box
    // =========================================================================

    inline bool overlaps(const Circle& circle, const Bounds2D& box)
    {
        Vector2 closest = glm::clamp(circle.center, box.min, box.max);
        Vector2 diff    = closest - circle.center;
        return glm::dot(diff, diff) <= (circle.radius * circle.radius);
    }

    inline bool overlaps(const Bounds2D& box, const Circle& circle)
    {
        return overlaps(circle, box);
    }

    inline bool overlaps(const Sphere& sphere, const Bounds3D& box)
    {
        Vector3 closest = glm::clamp(sphere.center, box.min, box.max);
        Vector3 diff    = closest - sphere.center;
        return glm::dot(diff, diff) <= (sphere.radius * sphere.radius);
    }

    inline bool overlaps(const Bounds3D& box, const Sphere& sphere)
    {
        return overlaps(sphere, box);
    }

    // =========================================================================
    // Frustum Culling & Containment
    // Convention: test_containment(Frustum, Target) / intersects(Frustum, Target)
    // =========================================================================

    inline ContainmentType test_containment(const Frustum& frustum, const Bounds3D& box)
    {
        Vector3 c = box.center();
        Vector3 e = box.extents();

        bool all_inside = true;
        for (const auto& plane : frustum.planes) {
            float r = e.x * std::abs(plane.normal.x) +
                      e.y * std::abs(plane.normal.y) +
                      e.z * std::abs(plane.normal.z);
            float d = plane.signed_distance(c);

            if (d < -r) {
                return ContainmentType::DISJOINT;
            }
            if (d < r) {
                all_inside = false;
            }
        }

        return all_inside ? ContainmentType::CONTAINS : ContainmentType::INTERSECTS;
    }

    inline bool intersects(const Frustum& frustum, const Bounds3D& box)
    {
        return test_containment(frustum, box) != ContainmentType::DISJOINT;
    }

    inline bool intersects(const Frustum& frustum, const Sphere& sphere)
    {
        for (const auto& plane : frustum.planes) {
            if (plane.signed_distance(sphere.center) < -sphere.radius) {
                return false;
            }
        }
        return true;
    }

} // namespace lyra

#endif // LYRA_ENGINE_UTILITIES_INTERSECT_H
