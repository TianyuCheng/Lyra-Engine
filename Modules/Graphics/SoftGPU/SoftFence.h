#pragma once

#ifndef LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTFENCE_H
#define LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTFENCE_H

#include "SoftCommon.h"

namespace lyra::softgpu
{
    struct SoftFenceState
    {
        Atomic<bool>              signaled{true};
        mutable Mutex             mutex;
        mutable ConditionVariable cv;
    };

    struct SoftFence
    {
        Ref<SoftFenceState> state;

        SoftFence() : state(make_ref<SoftFenceState>()) {}

        void wait() const;
        bool poll() const;
        void signal();
        void reset();
        void destroy() { state = nullptr; }
    };
} // namespace lyra::softgpu

#endif // LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTFENCE_H
