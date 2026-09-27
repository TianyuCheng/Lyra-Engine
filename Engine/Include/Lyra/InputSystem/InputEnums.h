#pragma once

#ifndef LYRA_ENGINE_INPUT_SYSTEM_INPUT_ENUMS_H
#define LYRA_ENGINE_INPUT_SYSTEM_INPUT_ENUMS_H

#include <cstdint>

namespace lyra
{
    /**
     * @brief high-level digital button actions mapped from physical device inputs.
     */
    enum struct InputAction : uint32_t
    {
        // directional movement
        MOVE_FORWARD,
        MOVE_BACKWARD,
        MOVE_LEFT,
        MOVE_RIGHT,
        MOVE_UP,   // fly up / elevate
        MOVE_DOWN, // fly down / descend

        // modifiers
        SPRINT, // boost / run (e.g. shift)
        WALK,

        // common gameplay actions
        JUMP,
        CROUCH,
        ATTACK,     // primary action / fire
        ATTACK_ALT, // secondary action / fire
        INTERACT,
        USE,
        RELOAD,

        // camera & navigation
        LOOK_ACTIVATE, // hold to enable camera look (rmb / lmb)
        PAUSE,
        CANCEL,  // escape
        CONFIRM, // enter / space

        COUNT
    };

    /**
     * @brief 1d scalar axes (range [-1.0, 1.0] or delta).
     */
    enum struct InputAxis : uint32_t
    {
        HORIZONTAL, // strafe left (-1) to right (+1)
        VERTICAL,   // move backward (-1) to forward (+1)
        ELEVATION,  // down (-1) to up (+1)
        ZOOM,       // scroll wheel delta
        COUNT
    };

    /**
     * @brief 2d composite vector axes.
     */
    enum struct InputAxis2D : uint32_t
    {
        MOVE, // x = horizontal (left/right), y = vertical (backward/forward)
        LOOK, // x = mouse delta x (yaw), y = mouse delta y (pitch)
        COUNT
    };

} // namespace lyra

#endif // LYRA_ENGINE_INPUT_SYSTEM_INPUT_ENUMS_H
