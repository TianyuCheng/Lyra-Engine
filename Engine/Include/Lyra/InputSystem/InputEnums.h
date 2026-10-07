#pragma once

#ifndef LYRA_ENGINE_INPUT_SYSTEM_INPUT_ENUMS_H
#define LYRA_ENGINE_INPUT_SYSTEM_INPUT_ENUMS_H

#include <cstdint>
#include <cstddef>
#include <Lyra/Utilities/Macros.h>

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
        CONFIRM, // enter

        // custom user operations
        CUSTOM_0,
        CUSTOM_1,
        CUSTOM_2,
        CUSTOM_3,
        CUSTOM_4,
        CUSTOM_5,
        CUSTOM_6,
        CUSTOM_7,

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

        // custom user axes
        CUSTOM_0,
        CUSTOM_1,
        CUSTOM_2,
        CUSTOM_3,

        COUNT
    };

    /**
     * @brief 2d composite vector axes.
     */
    enum struct InputAxis2D : uint32_t
    {
        MOVE, // x = horizontal (left/right), y = vertical (backward/forward)
        LOOK, // x = mouse delta x (yaw), y = mouse delta y (pitch)

        // custom user 2d axes
        CUSTOM_0,
        CUSTOM_1,
        CUSTOM_2,
        CUSTOM_3,

        COUNT
    };

    inline constexpr size_t CUSTOM_ACTION_COUNT  = 8;
    inline constexpr size_t CUSTOM_AXIS_1D_COUNT = 4;
    inline constexpr size_t CUSTOM_AXIS_2D_COUNT = 4;

    FORCE_INLINE constexpr auto custom_action(size_t index) -> InputAction
    {
        return static_cast<InputAction>(static_cast<size_t>(InputAction::CUSTOM_0) + index);
    }

    FORCE_INLINE constexpr auto custom_axis(size_t index) -> InputAxis
    {
        return static_cast<InputAxis>(static_cast<size_t>(InputAxis::CUSTOM_0) + index);
    }

    FORCE_INLINE constexpr auto custom_axis_2d(size_t index) -> InputAxis2D
    {
        return static_cast<InputAxis2D>(static_cast<size_t>(InputAxis2D::CUSTOM_0) + index);
    }

} // namespace lyra

#endif // LYRA_ENGINE_INPUT_SYSTEM_INPUT_ENUMS_H
