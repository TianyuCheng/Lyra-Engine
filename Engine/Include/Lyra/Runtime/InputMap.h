#pragma once

#ifndef LYRA_ENGINE_RUNTIME_INPUT_MAP_H
#define LYRA_ENGINE_RUNTIME_INPUT_MAP_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Windowing/WSIEnums.h>
#include <Lyra/Runtime/InputEnums.h>

namespace lyra
{
    /**
     * @brief unified physical button identifier supporting keyboard and mouse buttons.
     */
    struct DeviceButton
    {
        enum struct Type : uint8_t
        {
            KEY,
            MOUSE
        } type = Type::KEY;

        uint16_t code = 0;

        static constexpr DeviceButton key(KeyButton k)
        {
            return {Type::KEY, static_cast<uint16_t>(k)};
        }

        static constexpr DeviceButton mouse(MouseButton m)
        {
            return {Type::MOUSE, static_cast<uint16_t>(m)};
        }

        bool operator==(const DeviceButton& other) const = default;
    };

    /**
     * @brief 1d axis constructed from digital button pairs.
     */
    struct Axis1DComposite
    {
        SmallVector<DeviceButton, 2> positive;
        SmallVector<DeviceButton, 2> negative;
        float                        sensitivity = 1.0f;
    };

    /**
     * @brief 2d composite vector (e.g. wasd or arrow movement).
     */
    struct Axis2DComposite
    {
        SmallVector<DeviceButton, 2> up;
        SmallVector<DeviceButton, 2> down;
        SmallVector<DeviceButton, 2> left;
        SmallVector<DeviceButton, 2> right;
        bool                         normalize = true;
    };

} // namespace lyra

#endif // LYRA_ENGINE_RUNTIME_INPUT_MAP_H
