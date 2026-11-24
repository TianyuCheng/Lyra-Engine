#pragma once

#ifndef LYRA_ENGINE_RENDERING_FRAME_ARENA_H
#define LYRA_ENGINE_RENDERING_FRAME_ARENA_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Graphics/RHITypes.h>

namespace lyra
{
    /**
     * @brief Manages transient GPU allocations (bind groups, uniform staging) reset every frame.
     */
    class FrameArena
    {
    public:
        explicit FrameArena(uint frames_in_flight = 3, uint default_page_size = 4096);
        ~FrameArena();

        FrameArena(const FrameArena&)            = delete;
        FrameArena& operator=(const FrameArena&) = delete;

        FrameArena(FrameArena&&) noexcept;
        FrameArena& operator=(FrameArena&&) noexcept;

        void init(GPUDevice device, uint frames_in_flight = 3, uint default_page_size = 4096);
        void destroy();

        /// Allocates a transient bind group from the current frame's active heap.
        auto allocate(const GPUBindGroupDescriptor& descriptor, GPUDevice device = {}) -> GPUBindGroup;

        /// Advance to next frame slot and reset transient resources for that slot.
        void tick();

        [[nodiscard]] uint current_slot() const
        {
            return current_frame_slot;
        }

        [[nodiscard]] uint heap_count() const
        {
            return static_cast<uint>(bind_group_heaps.size());
        }

    private:
        GPUDevice                device;
        uint                     frames_in_flight   = 3;
        uint                     current_frame_slot = 0;
        uint                     page_size          = 4096;
        Vector<GPUBindGroupHeap> bind_group_heaps;
    };
} // namespace lyra

#endif // LYRA_ENGINE_RENDERING_FRAME_ARENA_H
