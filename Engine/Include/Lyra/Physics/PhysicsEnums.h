#pragma once

#ifndef LYRA_ENGINE_PHYSICS_PHYSICSENUMS_H
#define LYRA_ENGINE_PHYSICS_PHYSICSENUMS_H

#include <Lyra/Utilities/Stdint.h>

namespace lyra
{
    /**
     * @brief Motion behavior of a rigid body during simulation.
     */
    enum class MotionType : ubyte
    {
        Static,    ///< immovable object with infinite mass; zero per-frame integration cost.
        Dynamic,   ///< fully simulated by forces, impulses, gravity, and contact constraints.
        Kinematic, ///< moved strictly via transform/velocity code; pushes dynamic bodies, unaffected by forces.
    };

    /**
     * @brief Supported collision geometry primitive and cooked mesh shapes.
     */
    enum class ShapeType : ubyte
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
    enum class CollisionLayer : ushort
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
    enum class JointType : ubyte
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
    enum class CombineMode : ubyte
    {
        Average,
        Min,
        Max,
        Multiply,
    };

    /**
     * @brief Lifecycle phase of a contact constraint.
     */
    enum class ContactEventType : ubyte
    {
        Begin,
        Persist,
        End,
    };

    /**
     * @brief Lifecycle phase of a trigger volume overlap.
     */
    enum class TriggerEventType : ubyte
    {
        Enter,
        Exit,
    };

    /**
     * @brief Supported physics simulation backends.
     */
    enum class PhysicsBackend : ubyte
    {
        Jolt,
        Mock,
        Custom,
    };

} // namespace lyra

#endif // LYRA_ENGINE_PHYSICS_PHYSICSENUMS_H
