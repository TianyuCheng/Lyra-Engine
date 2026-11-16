#include "SoftFence.h"

using namespace lyra;
using namespace lyra::softgpu;

void SoftFence::wait() const
{
    if (!state) return;
    UniqueLock<Mutex> lock(state->mutex);
    state->cv.wait(lock, [this]() {
        return state->signaled.load(MemoryOrder::acquire);
    });
}

bool SoftFence::poll() const
{
    if (!state) return true;
    return state->signaled.load(MemoryOrder::acquire);
}

void SoftFence::signal()
{
    if (!state) return;
    {
        LockGuard<Mutex> lock(state->mutex);
        state->signaled.store(true, MemoryOrder::release);
    }
    state->cv.notify_all();
}

void SoftFence::reset()
{
    if (!state) return;
    LockGuard<Mutex> lock(state->mutex);
    state->signaled.store(false, MemoryOrder::release);
}
