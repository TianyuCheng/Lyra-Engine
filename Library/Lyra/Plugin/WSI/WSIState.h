#pragma once

#ifndef LYRA_LIBRARY_PLUGIN_WSI_STATE_H
#define LYRA_LIBRARY_PLUGIN_WSI_STATE_H

#include <chrono>

#include <Lyra/Common/Enums.h>
#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/Collections.h>
#include <Lyra/Plugin/WSI/WSIUtils.h>
#include <Lyra/Plugin/WSI/WSIEnums.h>
#include <Lyra/Plugin/WSI/WSIEvent.h>

namespace lyra
{
    struct MouseState
    {
        static constexpr size_t COUNT = magic_enum::enum_count<MouseButton>();

        MouseMoveEvent            position;
        MouseWheelEvent           scroll;
        Array<ButtonState, COUNT> status;
    };

    struct KeyboardState
    {
        static constexpr size_t COUNT = magic_enum::enum_count<KeyButton>();

        Array<ButtonState, COUNT> status;
    };

    struct InputState
    {
        MouseState    mouse;
        KeyboardState keyboard;

        void update(const WindowHandle& handle);
    };

    struct WindowInput
    {
    public:
        using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;

        explicit WindowInput();

        void update(const WindowHandle& handle);

        auto get_delta_time() const -> float { return delta_time; }
        auto get_elapsed_time() const -> TimePoint { return elapsed_time; }

        bool is_mouse_moved(MouseButton button) const;
        bool is_mouse_dragged(MouseButton button) const;
        bool is_mouse_pressed(MouseButton button) const;
        bool is_mouse_released(MouseButton button) const;

        bool is_key_down(KeyButton key) const;     // when key is pressed down and hold (simply the current status)
        bool is_key_pressed(KeyButton key) const;  // exactly once when key is pressed down
        bool is_key_released(KeyButton key) const; // exactly once when key is released up

    private:
        auto current_state() const -> const InputState& { return states[state_index]; }
        auto previous_state() const -> const InputState& { return states[(state_index + 1) % 2]; }

    private:
        InputState states[2];
        uint       state_index = 0;
        float      delta_time  = 0.0f; // in seconds
        TimePoint  elapsed_time;
    };

} // namespace lyra

#endif // LYRA_LIBRARY_PLUGIN_WSI_STATE_H
