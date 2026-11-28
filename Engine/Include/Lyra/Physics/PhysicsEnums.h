#pragma once

#ifndef LYRA_ENGINE_PHYSICS_PHYSICSENUMS_H
#define LYRA_ENGINE_PHYSICS_PHYSICSENUMS_H

#include <Lyra/Utilities/Stdint.h>

namespace lyra
{
    /**
     * @brief Motion behavior of a rigid body during simulation.
     */
    enum struct MotionType : ubyte
    {
        Static,    ///< immovable object with infinite mass; zero per-frame integration cost.
        Dynamic,   ///< fully simulated by forces, impulses, gravity, and contact constraints.
        Kinematic, ///< moved strictly via transform/velocity code; pushes dynamic bodies, unaffected by forces.
    };

    /**
     * @brief Supported collision geometry primitive and cooked mesh shapes.
     */
    enum struct ShapeType : ubyte
    {
        Box,
        Sphere,
        Capsule,
        Cylinder,
        ConvexHull,
        TriangleMesh,
        Compound,
    };

    /**
     * @brief Standard collision filtering layer identifiers.
     */
    enum struct CollisionLayer : ushort
    {
        NonMoving   = 0, ///< static scenery (walls, floors, terrain).
        Moving      = 1, ///< dynamic rigid bodies (crates, barrels, props).
        Trigger     = 2, ///< non-physical overlap detector zones.
        Character   = 3, ///< virtual kinematic character controllers.
        Debris      = 4, ///< minor visual debris ignoring character collisions.
        CustomStart = 5, ///< base index for game-specific custom layers.
    };

    /**
     * @brief Physics constraint and joint connection types.
     */
    enum struct JointType : ubyte
    {
        Fixed,
        Point,
        Hinge,
        Slider,
        Distance,
        Cone,
    };

    /**
     * @brief Combine mode for friction and restitution calculations at contact points.
     */
    enum struct CombineMode : ubyte
    {
        Average,
        Min,
        Max,
        Multiply,
    };

    /**
     * @brief Lifecycle phase of a contact constraint.
     */
    enum struct ContactEventType : ubyte
    {
        Begin,
        Persist,
        End,
    };

    /**
     * @brief Lifecycle phase of a trigger volume overlap.
     */
    enum struct TriggerEventType : ubyte
    {
        Enter,
        Exit,
    };

    /**
     * @brief Supported physics simulation backends.
     */
    enum struct PhysicsBackend : ubyte
    {
        Jolt,
        Mock,
        Custom,
    };

} // namespace lyra

#endif // LYRA_ENGINE_PHYSICS_PHYSICSENUMS_H
