#pragma once

#ifndef LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTTHREADPOOL_H
#define LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTTHREADPOOL_H

#include <thread>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Utilities/Sync.h>

namespace lyra::softgpu
{
    struct SoftTaskDesc
    {
        void (*fn)(void* ctx, uint idx) = nullptr;
        void* ctx                       = nullptr;

        void execute(uint idx) const
        {
            if (fn) fn(ctx, idx);
        }
    };

    class SoftThreadPool
    {
    public:
        SoftThreadPool() = default;
        ~SoftThreadPool();

        void init(uint thread_count = 0);
        void shutdown();

        template <typename F>
        void parallel_for(uint count, const F& func)
        {
            if (count == 0) return;
            if (count == 1 || workers.empty()) {
                for (uint i = 0; i < count; ++i) {
                    func(i);
                }
                return;
            }

            auto trampoline = [](void* ctx, uint idx) {
                (*static_cast<const F*>(ctx))(idx);
            };
            dispatch_work(count, SoftTaskDesc{trampoline, const_cast<void*>(static_cast<const void*>(&func))});
        }

        uint thread_count() const { return static_cast<uint>(workers.size()) + 1; }

    private:
        void dispatch_work(uint count, SoftTaskDesc task);
        void worker_loop(uint worker_id);

        Vector<std::thread> workers;
        Mutex               mutex;
        ConditionVariable   cv_work;
        ConditionVariable   cv_done;

        SoftTaskDesc current_task{};
        Atomic<uint> next_index{0};
        Atomic<uint> remaining{0};
        Atomic<uint> active_workers{0};
        uint         total_count{0};
        uint         generation{0};
        Atomic<bool> stop{false};
    };
} // namespace lyra::softgpu

#endif // LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTTHREADPOOL_H
