#pragma once

#ifndef LYRA_MODULES_GRAPHICS_SOFTWARE_SOFTFENCE_H
#define LYRA_MODULES_GRAPHICS_SOFTWARE_SOFTFENCE_H

#include "SoftCommon.h"

namespace lyra::softgpu
{
    struct SoftFenceState
    {
        std::atomic<bool>         signaled{true};
        mutable Mutex             mutex;
        mutable ConditionVariable cv;
    };

    struct SoftFence
    {
        Ref<SoftFenceState> state;

        SoftFence() : state(make_ref<SoftFenceState>()) {}

        auto wait() const -> void;
        auto poll() const -> bool;
        auto signal() -> void;
        auto reset() -> void;
    };
} // namespace lyra::softgpu

#endif // LYRA_MODULES_GRAPHICS_SOFTWARE_SOFTFENCE_H
