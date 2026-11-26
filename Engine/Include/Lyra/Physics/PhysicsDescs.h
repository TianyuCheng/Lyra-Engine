#pragma once

#ifndef LYRA_ENGINE_PHYSICS_PHYSICSDESCS_H
#define LYRA_ENGINE_PHYSICS_PHYSICSDESCS_H

#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Physics/PhysicsEnums.h>
#include <Lyra/Physics/PhysicsTypes.h>

namespace lyra
{
    /**
     * @brief Configuration for initializing a physics world/scene simulation instance.
     */
    struct PhysicsWorldDescriptor
    {
        Vector3 gravity                 = Vector3(0.0f, -9.81f, 0.0f); ///< global gravity acceleration.
        uint    max_bodies              = 16384;                       ///< maximum concurrent rigid bodies.
        uint    max_body_pairs          = 65536;                       ///< maximum broadphase overlap pairs.
        uint    max_contact_constraints = 32768;                       ///< maximum active contact manifolds.
        uint    temp_allocator_size_mb  = 32;                          ///< scratch allocator memory in MB.
    };

    /**
     * @brief Surface friction and restitution properties.
     */
    struct PhysicsMaterialDescriptor
    {
        float       static_friction     = 0.5f;                 ///< static friction coefficient >= 0.
        float       dynamic_friction    = 0.5f;                 ///< dynamic kinetic friction coefficient >= 0.
        float       restitution         = 0.0f;                 ///< bounciness coefficient in [0, 1].
        CombineMode friction_combine    = CombineMode::Average; ///< combine rule for friction.
        CombineMode restitution_combine = CombineMode::Average; ///< combine rule for restitution.
    };

    /**
     * @brief Construction parameters for creating a rigid body in the physics world.
     */
    struct RigidBodyDescriptor
    {
        MotionType            motion_type      = MotionType::Dynamic;
        PhysicsShapeHandle    shape            = {};
        PhysicsMaterialHandle material         = {};
        Vector3               position         = Vector3(0.0f);
        Quaternion            rotation         = glm::identity<Quaternion>();
        Vector3               linear_velocity  = Vector3(0.0f);
        Vector3               angular_velocity = Vector3(0.0f);
        float                 mass             = 1.0f;
        float                 linear_damping   = 0.05f;
        float                 angular_damping  = 0.05f;
        float                 gravity_factor   = 1.0f;
        bool                  enable_ccd       = false; ///< continuous collision detection (swept).
        bool                  is_sensor        = false; ///< acts as trigger volume without physical force.
        ushort                collision_layer  = static_cast<ushort>(CollisionLayer::Moving);
        ulong                 user_data        = 0; ///< user payload (e.g. entt::entity or GUID).
    };

    /**
     * @brief Construction parameters for a virtual kinematic character controller.
     */
    struct CharacterDescriptor
    {
        Vector3    position        = Vector3(0.0f);
        Quaternion rotation        = glm::identity<Quaternion>();
        float      capsule_radius  = 0.3f;
        float      capsule_height  = 1.8f;
        float      max_slope_angle = glm::radians(45.0f); ///< max walkable angle before sliding.
        float      max_step_height = 0.3f;                ///< max stair/curb step obstacle height.
        Vector3    up_vector       = Vector3(0.0f, 1.0f, 0.0f);
        ushort     collision_layer = static_cast<ushort>(CollisionLayer::Character);
        ulong      user_data       = 0;
    };

    /**
     * @brief Construction parameters for mechanical joints and constraints.
     */
    struct JointDescriptor
    {
        JointType         type             = JointType::Fixed;
        PhysicsBodyHandle body_a           = {};
        PhysicsBodyHandle body_b           = {};
        Vector3           anchor_a         = Vector3(0.0f);
        Vector3           anchor_b         = Vector3(0.0f);
        Vector3           axis_a           = Vector3(0.0f, 1.0f, 0.0f);
        Vector3           axis_b           = Vector3(0.0f, 1.0f, 0.0f);
        float             min_limit        = 0.0f;
        float             max_limit        = 0.0f;
        float             spring_frequency = 0.0f;
        float             spring_damping   = 0.0f;
    };

} // namespace lyra

#endif // LYRA_ENGINE_PHYSICS_PHYSICSDESCS_H
