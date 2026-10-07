#pragma once

#ifndef LYRA_ENGINE_INPUT_SYSTEM_INPUT_MAP_H
#define LYRA_ENGINE_INPUT_SYSTEM_INPUT_MAP_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Macros.h>
#include <Lyra/Utilities/BitFlags.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Windowing/WSIEnums.h>
#include <Lyra/InputSystem/InputEnums.h>

namespace lyra
{
    /**
     * @brief modifier key flags for keyboard combinations.
     */
    enum struct ModifierKey : uint
    {
        NONE  = 0x0,
        SHIFT = 0x1,
        CTRL  = 0x2,
        ALT   = 0x4,
        SUPER = 0x8,
    };
} // namespace lyra

ENABLE_BIT_FLAGS(lyra::ModifierKey);

namespace lyra
{
    using ModifierKeys = BitFlags<ModifierKey>;

    /**
     * @brief unified physical button identifier supporting keyboard and mouse buttons.
     */
    struct DeviceButton
    {
        // clang-format off
        enum struct Type : uint8_t { NONE = 0, KEY, MOUSE } type = Type::NONE;
        // clang-format on

        uint16_t code = 0;

        constexpr DeviceButton() = default;
        constexpr DeviceButton(Type type, uint16_t code) : type(type), code(code) {}

        static constexpr DeviceButton key(KeyButton k)
        {
            return {Type::KEY, static_cast<uint16_t>(k)};
        }

        static constexpr DeviceButton mouse(MouseButton m)
        {
            return {Type::MOUSE, static_cast<uint16_t>(m)};
        }

        constexpr bool valid() const { return type != Type::NONE; }

        bool operator==(const DeviceButton& other) const = default;
    };

    /**
     * @brief physical input chord requiring a primary button plus optional modifiers and/or secondary button.
     */
    struct ButtonChord
    {
        DeviceButton primary   = {};
        ModifierKeys modifiers = ModifierKey::NONE;
        DeviceButton extra     = {};

        constexpr ButtonChord() = default;

        // implicit conversion from DeviceButton
        constexpr ButtonChord(DeviceButton btn)
            : primary(btn) {}

        constexpr ButtonChord(DeviceButton btn, ModifierKeys mods)
            : primary(btn), modifiers(mods) {}

        constexpr ButtonChord(DeviceButton btn, DeviceButton extra_btn)
            : primary(btn), extra(extra_btn) {}

        constexpr ButtonChord(DeviceButton btn, ModifierKeys mods, DeviceButton extra_btn)
            : primary(btn), modifiers(mods), extra(extra_btn) {}

        static constexpr ButtonChord key(KeyButton k, ModifierKeys mods = ModifierKey::NONE)
        {
            return {DeviceButton::key(k), mods};
        }

        static constexpr ButtonChord mouse(MouseButton m, ModifierKeys mods = ModifierKey::NONE)
        {
            return {DeviceButton::mouse(m), mods};
        }

        static constexpr ButtonChord chord(DeviceButton btn1, DeviceButton btn2)
        {
            return {btn1, btn2};
        }

        static constexpr ButtonChord chord(DeviceButton btn1, ModifierKeys mods, DeviceButton btn2)
        {
            return {btn1, mods, btn2};
        }

        constexpr bool valid() const { return primary.valid(); }

        bool operator==(const ButtonChord& other) const = default;
    };

    /**
     * @brief alias for developers preferring combo terminology.
     */
    using ButtonCombo = ButtonChord;

    /**
     * @brief 1d axis constructed from digital button/chord pairs.
     */
    struct Axis1DComposite
    {
        SmallVector<ButtonChord, 2> positive;
        SmallVector<ButtonChord, 2> negative;
        float                       sensitivity = 1.0f;
    };

    /**
     * @brief 2d composite vector (e.g. wasd or arrow movement).
     */
    struct Axis2DComposite
    {
        SmallVector<ButtonChord, 2> up;
        SmallVector<ButtonChord, 2> down;
        SmallVector<ButtonChord, 2> left;
        SmallVector<ButtonChord, 2> right;
        bool                        normalize = true;
    };

} // namespace lyra

#endif // LYRA_ENGINE_INPUT_SYSTEM_INPUT_MAP_H
