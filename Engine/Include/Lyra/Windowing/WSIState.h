#pragma once

#ifndef LYRA_ENGINE_WINDOWING_WSISTATE_H
#define LYRA_ENGINE_WINDOWING_WSISTATE_H

#include <chrono>

#include <Lyra/Utilities/Enums.h>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Utilities/Math.h>
#include <Lyra/Windowing/WSIUtils.h>
#include <Lyra/Windowing/WSIEnums.h>
#include <Lyra/Windowing/WSIEvent.h>

namespace lyra
{
    struct FileState
    {
        using Files = SmallVector<String, 4>;

        Files entries;
    };

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
        FileState     files;
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

        bool is_mouse_down(MouseButton button) const;
        bool is_mouse_moved(MouseButton button) const;
        bool is_mouse_dragged(MouseButton button) const;
        bool is_mouse_pressed(MouseButton button) const;
        bool is_mouse_released(MouseButton button) const;

        auto get_mouse_position() const -> Vector2;
        auto get_mouse_delta() const -> Vector2;
        auto get_mouse_scroll() const -> Vector2;

        bool is_key_down(KeyButton key) const;     // when key is pressed down and hold (simply the current status)
        bool is_key_pressed(KeyButton key) const;  // exactly once when key is pressed down
        bool is_key_released(KeyButton key) const; // exactly once when key is released up

        bool has_dropped_files() const;
        auto get_dropped_files() const -> detail::typed_view<String>;

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

#endif // LYRA_ENGINE_WINDOWING_WSISTATE_H
