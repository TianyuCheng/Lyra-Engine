#pragma once

#ifndef LYRA_ENGINE_PHYSICS_PHYSICSQUERIES_H
#define LYRA_ENGINE_PHYSICS_PHYSICSQUERIES_H

#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Physics/PhysicsTypes.h>

namespace lyra
{
    /**
     * @brief Input parameters for a spatial raycast query.
     */
    struct RaycastQuery
    {
        Vector3 origin;
        Vector3 direction;
        float   max_distance = 1000.0f;
        uint    layer_mask   = 0xFFFFFFFF; ///< collision layer bitmask filter.
    };

    /**
     * @brief Detailed intersection result from a raycast or shape sweep.
     */
    struct RaycastHit
    {
        Vector3           position  = Vector3(0.0f);
        Vector3           normal    = Vector3(0.0f, 1.0f, 0.0f);
        float             distance  = 0.0f;
        PhysicsBodyHandle body      = {};
        ulong             user_data = 0;     ///< user payload from body descriptor (entity ID).
        bool              has_hit   = false;
    };

    /**
     * @brief Input parameters for casting a shape along a linear direction.
     */
    struct ShapeSweepQuery
    {
        PhysicsShapeHandle shape;
        Vector3            start_position = Vector3(0.0f);
        Quaternion         start_rotation = glm::identity<Quaternion>();
        Vector3            direction;
        float              max_distance   = 100.0f;
        uint               layer_mask     = 0xFFFFFFFF;
    };

    /**
     * @brief Input parameters for spherical volume overlap query.
     */
    struct OverlapSphereQuery
    {
        Vector3 center     = Vector3(0.0f);
        float   radius     = 1.0f;
        uint    layer_mask = 0xFFFFFFFF;
    };

    /**
     * @brief Input parameters for oriented bounding box volume overlap query.
     */
    struct OverlapBoxQuery
    {
        Vector3    center       = Vector3(0.0f);
        Vector3    half_extents = Vector3(0.5f);
        Quaternion rotation     = glm::identity<Quaternion>();
        uint       layer_mask   = 0xFFFFFFFF;
    };

} // namespace lyra

#endif // LYRA_ENGINE_PHYSICS_PHYSICSQUERIES_H
