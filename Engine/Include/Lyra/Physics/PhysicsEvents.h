#pragma once

#ifndef LYRA_ENGINE_PHYSICS_PHYSICSEVENTS_H
#define LYRA_ENGINE_PHYSICS_PHYSICSEVENTS_H

#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Physics/PhysicsEnums.h>
#include <Lyra/Physics/PhysicsTypes.h>

namespace lyra
{
    /**
     * @brief A single physical contact manifold point.
     */
    struct ContactPoint
    {
        Vector3 position;
        Vector3 normal;   ///< surface normal pointing from body A to body B.
        float   depth;    ///< penetration depth.
        float   impulse;  ///< applied normal constraint impulse magnitude.
    };

    /**
     * @brief Impact event emitted when two rigid bodies touch, persist, or separate.
     */
    struct ContactEvent
    {
        ContactEventType  type        = ContactEventType::Begin;
        PhysicsBodyHandle body_a      = {};
        PhysicsBodyHandle body_b      = {};
        ulong             user_data_a = 0;
        ulong             user_data_b = 0;
        Vector3           normal      = Vector3(0.0f, 1.0f, 0.0f);
        ContactPoint      point       = {};
    };

    /**
     * @brief Overlap event emitted when a body enters or exits a sensor/trigger volume.
     */
    struct TriggerEvent
    {
        TriggerEventType  type              = TriggerEventType::Enter;
        PhysicsBodyHandle trigger_body      = {};
        PhysicsBodyHandle other_body        = {};
        ulong             trigger_user_data = 0;
        ulong             other_user_data   = 0;
    };

} // namespace lyra

#endif // LYRA_ENGINE_PHYSICS_PHYSICSEVENTS_H
