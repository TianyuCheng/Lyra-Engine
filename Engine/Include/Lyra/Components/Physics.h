#pragma once

#ifndef LYRA_ENGINE_COMPONENTS_PHYSICS_H
#define LYRA_ENGINE_COMPONENTS_PHYSICS_H

#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Macros.h>
#include <Lyra/Assets/Format/MeshAsset.h>
#include <Lyra/Physics/PhysicsEnums.h>
#include <Lyra/Physics/PhysicsTypes.h>

namespace lyra
{
    /**
     * @brief Component representing physical rigid body dynamics and simulation properties.
     */
    struct [[lyra::component("RigidBody", category = "Physics", icon = "LYRA_ICON_SETTINGS")]] RigidBodyComponent
    {
        [[lyra::label("Motion Type")]]
        MotionType motion_type = MotionType::Dynamic;

        [[lyra::label("Mass"), lyra::range(0.001f, 100000.0f)]]
        float mass = 1.0f;

        [[lyra::label("Friction"), lyra::range(0.0f, 2.0f)]]
        float friction = 0.5f;

        [[lyra::label("Restitution"), lyra::range(0.0f, 1.0f)]]
        float restitution = 0.0f;

        [[lyra::label("Linear Damping"), lyra::range(0.0f, 1.0f)]]
        float linear_damping = 0.05f;

        [[lyra::label("Angular Damping"), lyra::range(0.0f, 1.0f)]]
        float angular_damping = 0.05f;

        [[lyra::label("Gravity Factor")]]
        float gravity_factor = 1.0f;

        [[lyra::label("Continuous Collision (CCD)")]]
        bool enable_ccd = false;

        [[lyra::label("Collision Layer")]]
        ushort collision_layer = static_cast<ushort>(CollisionLayer::Moving);

        [[lyra::hidden]]
        PhysicsBodyHandle runtime_body = {};
    };

    /**
     * @brief Component representing collision geometry bounds attached to an entity.
     */
    struct [[lyra::component("Collider", category = "Physics", icon = "LYRA_ICON_MESH")]] ColliderComponent
    {
        [[lyra::label("Shape Type")]]
        ShapeType shape_type = ShapeType::Box;

        [[lyra::label("Box Half Extents")]]
        Vector3 half_extents = Vector3(0.5f);

        [[lyra::label("Radius"), lyra::range(0.01f, 1000.0f)]]
        float radius = 0.5f;

        [[lyra::label("Height"), lyra::range(0.01f, 1000.0f)]]
        float height = 1.0f;

        [[lyra::label("Center Offset")]]
        Vector3 center_offset = Vector3(0.0f);

        [[lyra::label("Is Trigger (Sensor)")]]
        bool is_trigger = false;

        [[lyra::label("Custom Mesh Asset")]]
        MeshAssetHandle custom_mesh = {};

        [[lyra::hidden]]
        PhysicsShapeHandle runtime_shape = {};
    };

    /**
     * @brief Component for player/NPC kinematic character locomotion with slope and stair handling.
     */
    struct [[lyra::component("CharacterController", category = "Physics", icon = "LYRA_ICON_SETTINGS")]] CharacterControllerComponent
    {
        [[lyra::label("Radius"), lyra::range(0.05f, 5.0f)]]
        float radius = 0.3f;

        [[lyra::label("Height"), lyra::range(0.1f, 10.0f)]]
        float height = 1.8f;

        [[lyra::label("Max Slope Angle")]]
        float max_slope_angle_degrees = 45.0f;

        [[lyra::label("Step Height")]]
        float step_height = 0.3f;

        [[lyra::label("Collision Layer")]]
        ushort collision_layer = static_cast<ushort>(CollisionLayer::Character);

        [[lyra::hidden]]
        PhysicsCharacterHandle runtime_character = {};
    };

} // namespace lyra

#endif // LYRA_ENGINE_COMPONENTS_PHYSICS_H
