#pragma once

#ifndef LYRA_ENGINE_RENDERING_FRAME_ARENA_H
#define LYRA_ENGINE_RENDERING_FRAME_ARENA_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Graphics/RHITypes.h>

namespace lyra
{
    /**
     * @brief A sub-allocated slice of a GPU buffer with a direct CPU-mapped pointer.
     */
    struct GPUBufferSlice
    {
        GPUBufferHandle buffer{};
        GPUSize64       offset      = 0;
        GPUSize64       size        = 0;
        void*           mapped_data = nullptr;

        auto valid() const -> bool
        {
            return buffer.valid() && mapped_data != nullptr;
        }

        template <typename T>
        [[nodiscard]] auto as() const -> T*
        {
            return reinterpret_cast<T*>(mapped_data);
        }

        FORCE_INLINE operator GPUBufferBinding() const
        {
            return GPUBufferBinding{buffer, offset, size};
        }
    };

    /**
     * @brief Manages transient GPU allocations (bind groups, uniform staging) reset every frame.
     */
    class FrameArena
    {
    public:
        explicit FrameArena(uint frames_in_flight = 3, uint default_heap_page_size = 4096, uint default_uniform_page_size = 65536);
        ~FrameArena();

        FrameArena(const FrameArena&)            = delete;
        FrameArena& operator=(const FrameArena&) = delete;

        FrameArena(FrameArena&&) noexcept;
        FrameArena& operator=(FrameArena&&) noexcept;

        void init(GPUDevice device, uint frames_in_flight = 3, uint default_heap_page_size = 4096, uint default_uniform_page_size = 65536);
        void destroy();

        /// Allocates a transient bind group from the current frame's active heap.
        auto allocate(const GPUBindGroupDescriptor& descriptor, GPUDevice device = {}) -> GPUBindGroup;

        /// Allocates an uninitialized slice of transient uniform memory for the current frame slot.
        auto allocate_uniform(usize size, usize alignment = 256, GPUDevice device = {}) -> GPUBufferSlice;

        /// Allocates a transient uniform slice and copies the provided data into it immediately.
        template <typename T>
        auto create_uniform(const T& data, usize alignment = 256, GPUDevice device = {}) -> GPUBufferSlice
        {
            auto slice = allocate_uniform(sizeof(T), alignment, device);
            if (slice.mapped_data) {
                std::memcpy(slice.mapped_data, &data, sizeof(T));
            }
            return slice;
        }

        /// Allocates a transient uniform slice and copies raw buffer data into it immediately.
        auto create_uniform(const void* data, usize size, usize alignment = 256, GPUDevice device = {}) -> GPUBufferSlice
        {
            auto slice = allocate_uniform(size, alignment, device);
            if (slice.mapped_data && data && size > 0) {
                std::memcpy(slice.mapped_data, data, size);
            }
            return slice;
        }

        /// Advance to next frame slot and reset transient resources for that slot.
        void tick();

        [[nodiscard]] auto current_slot() const -> uint
        {
            return current_frame_slot;
        }

        [[nodiscard]] auto heap_count() const -> uint
        {
            return static_cast<uint>(bind_group_heaps.size());
        }

        [[nodiscard]] auto uniform_page_count() const -> uint;

    private:
        struct UniformPage
        {
            GPUBuffer buffer;
            void*     mapped_ptr = nullptr;
            usize     capacity   = 0;
        };

        struct UniformSlot
        {
            Vector<UniformPage> pages;
            usize               current_page   = 0;
            usize               current_offset = 0;
        };

        GPUDevice                device;
        uint                     frames_in_flight   = 3;
        uint                     current_frame_slot = 0;
        uint                     heap_page_size     = 4096;
        uint                     uniform_page_size  = 65536;
        Vector<GPUBindGroupHeap> bind_group_heaps;
        Vector<UniformSlot>      uniform_slots;
    };
} // namespace lyra

#endif // LYRA_ENGINE_RENDERING_FRAME_ARENA_H
