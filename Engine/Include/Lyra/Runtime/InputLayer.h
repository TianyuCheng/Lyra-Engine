#pragma once

#ifndef LYRA_ENGINE_RUNTIME_INPUT_LAYER_H
#define LYRA_ENGINE_RUNTIME_INPUT_LAYER_H

#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Runtime/Application.h>
#include <Lyra/Runtime/InputEnums.h>
#include <Lyra/Runtime/InputMap.h>

namespace lyra
{
    struct WindowInput;

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

        // axis queries
        float   get_axis(InputAxis axis) const;
        Vector2 get_axis_2d(InputAxis2D axis) const;

        // binding manipulation
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
        void update(const WindowInput* raw_input, float dt);

    private:
        struct ActionState
        {
            bool current  = false;
            bool previous = false;
        };

        static constexpr size_t ACTION_COUNT  = static_cast<size_t>(InputAction::COUNT);
        static constexpr size_t AXIS_1D_COUNT = static_cast<size_t>(InputAxis::COUNT);
        static constexpr size_t AXIS_2D_COUNT = static_cast<size_t>(InputAxis2D::COUNT);

        Array<SmallVector<DeviceButton, 3>, ACTION_COUNT> action_bindings;
        Array<ActionState, ACTION_COUNT>                  action_states;

        Array<Axis1DComposite, AXIS_1D_COUNT> axis_1d_bindings;
        Array<float, AXIS_1D_COUNT>           axis_1d_values;

        Array<Axis2DComposite, AXIS_2D_COUNT> axis_2d_bindings;
        Array<Vector2, AXIS_2D_COUNT>         axis_2d_values;
    };

    /**
     * @brief application layer hosting and updating the input management system.
     */
    struct InputLayer
    {
    public:
        explicit InputLayer();

        void bind(Application& app);
        void update(AppContext& context);

        auto&       get_manager() { return manager; }
        const auto& get_manager() const { return manager; }

    private:
        InputManager manager;
    };

} // namespace lyra

#endif // LYRA_ENGINE_RUNTIME_INPUT_LAYER_H
