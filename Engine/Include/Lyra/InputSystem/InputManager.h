#pragma once

#ifndef LYRA_ENGINE_INPUT_SYSTEM_INPUT_MANAGER_H
#define LYRA_ENGINE_INPUT_SYSTEM_INPUT_MANAGER_H

#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/InputSystem/InputMap.h>
#include <Lyra/InputSystem/InputEnums.h>

namespace lyra
{
    struct WindowInput;

    struct InputFilter
    {
        bool block_mouse    = false;
        bool block_keyboard = false;
    };

    /**
     * @brief manager evaluating abstract input actions and composite axes from physical inputs.
     */
    struct InputManager
    {
    public:
        explicit InputManager();

        // action queries
        bool is_action_down(InputAction action) const;
        bool is_action_pressed(InputAction action) const;
        bool is_action_released(InputAction action) const;

        // custom action queries & binding helpers
        bool is_custom_action_down(size_t index) const;
        bool is_custom_action_pressed(size_t index) const;
        bool is_custom_action_released(size_t index) const;
        void bind_custom_action(size_t index, ButtonChord chord);
        void bind_custom_action(size_t index, DeviceButton button);
        void clear_custom_action(size_t index);

        // named action aliases
        void register_action_alias(StringView name, InputAction action);
        bool is_action_down(StringView name) const;
        bool is_action_pressed(StringView name) const;
        bool is_action_released(StringView name) const;
        void bind_action(StringView name, ButtonChord chord);
        void bind_action(StringView name, DeviceButton button);
        auto get_action_by_name(StringView name) const -> Optional<InputAction>;

        // axis queries
        auto get_axis(InputAxis axis) const -> float;
        auto get_axis_2d(InputAxis2D axis) const -> Vector2;

        // custom axis queries & binding helpers
        auto get_custom_axis(size_t index) const -> float;
        void bind_custom_axis_1d(size_t index, const Axis1DComposite& composite);

        auto get_custom_axis_2d(size_t index) const -> Vector2;
        void bind_custom_axis_2d(size_t index, const Axis2DComposite& composite);

        // input gating & state reset
        void set_enabled(bool enabled);
        bool is_enabled() const { return enabled; }
        void reset();

        // binding manipulation
        void bind_action(InputAction action, ButtonChord chord);
        void bind_action(InputAction action, DeviceButton button);
        void clear_action(InputAction action);
        void clear_actions();

        void bind_axis_1d(InputAxis axis, const Axis1DComposite& composite);
        void clear_axis_1d(InputAxis axis);
        void clear_axes_1d();

        void bind_axis_2d(InputAxis2D axis, const Axis2DComposite& composite);
        void clear_axis_2d(InputAxis2D axis);
        void clear_axes_2d();

        void set_default_bindings();
        void clear_all_bindings();

        // evaluation loop
        void update(const WindowInput* raw_input, float dt, const InputFilter& filter = {});

    private:
        struct ActionState
        {
            bool current  = false;
            bool previous = false;
        };

        static constexpr size_t ACTION_COUNT  = static_cast<size_t>(InputAction::COUNT);
        static constexpr size_t AXIS_1D_COUNT = static_cast<size_t>(InputAxis::COUNT);
        static constexpr size_t AXIS_2D_COUNT = static_cast<size_t>(InputAxis2D::COUNT);

        Array<SmallVector<ButtonChord, 3>, ACTION_COUNT> action_bindings;
        Array<ActionState, ACTION_COUNT>                 action_states;

        Array<Axis1DComposite, AXIS_1D_COUNT> axis_1d_bindings;
        Array<float, AXIS_1D_COUNT>           axis_1d_values;

        Array<Axis2DComposite, AXIS_2D_COUNT> axis_2d_bindings;
        Array<Vector2, AXIS_2D_COUNT>         axis_2d_values;

        HashMap<String, InputAction> action_aliases;

        bool enabled = true;
    };

} // namespace lyra

#endif // LYRA_ENGINE_INPUT_SYSTEM_INPUT_MANAGER_H
