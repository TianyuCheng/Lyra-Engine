#pragma once

#ifndef LYRA_LIBRARY_PLUGIN_WSI_ENUMS_H
#define LYRA_LIBRARY_PLUGIN_WSI_ENUMS_H

#include <Lyra/Common/Stdint.h>

namespace lyra
{
    enum struct WindowFlag : uint
    {
        DECORATED  = 0x1,
        MAXIMIZED  = 0x2,
        FULLSCREEN = 0x4,
    };

    enum struct WindowEvent : uint
    {
        START,  // window opened
        CLOSE,  // window closed
        TIMER,  // fixed update internal
        UPDATE, // regular update (same rate as render)
        RENDER, // render frame
        RESIZE, // window resized
    };

    enum struct ButtonState : uint
    {
        OFF,
        ON,
    };

    enum struct MouseButton : uint
    {
        LEFT,
        RIGHT,
        MIDDLE,
    };

    // clang-format off
    enum struct KeyButton : uint
    {
        // character keys
        A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

        // numerical keys
        D0, D1, D2, D3, D4, D5, D6, D7, D8, D9,

        // non-alphanumerical ascii keys
        APOSTROPHE, COMMA, MINUS, PERIOD, SLASH, BACKSLASH, SEMICOLON, EQUAL,
        LEFT_BRACKET, RIGHT_BRACKET, GRAVE_ACCENT,
        LAST_PRINTABLE_KEY = GRAVE_ACCENT,

        // function keys
        F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

        // arrow keys
        UP, DOWN, LEFT, RIGHT,

        // modifier keys
        ALT, CTRL, SHIFT, SUPER,

        // misc keys
        TAB, ESC, SPACE, BACKSPACE, DEL, ENTER, PAGE_UP, PAGE_DOWN, HOME, END,
        PAUSE, CAPS_LOCK, SCROLL_LOCK, NUM_LOCK, PRINT_SCREEN,
    };
    // clang-format on

} // namespace lyra

#endif // LYRA_LIBRARY_PLUGIN_WSI_ENUMS_H
