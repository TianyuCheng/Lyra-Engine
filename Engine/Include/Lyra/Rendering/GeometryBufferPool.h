#pragma once

#ifndef LYRA_ENGINE_RENDERING_GEOMETRY_BUFFER_POOL_H
#define LYRA_ENGINE_RENDERING_GEOMETRY_BUFFER_POOL_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Rendering/Geometry.h>

namespace lyra
{
    class ResourceRetire;

    /**
     * @brief Manages large pooled GPU buffer slabs and sub-allocates aligned BufferSlice ranges.
     */
    class GeometryBufferPool
    {
    public:
        static constexpr GPUSize64 DEFAULT_SLAB_SIZE = 32 * 1024 * 1024; // 32 MB

        explicit GeometryBufferPool(ResourceRetire& resource_retire, GPUSize64 slab_size = DEFAULT_SLAB_SIZE);
        virtual ~GeometryBufferPool();

        GeometryBufferPool(const GeometryBufferPool&)            = delete;
        GeometryBufferPool& operator=(const GeometryBufferPool&) = delete;

        GeometryBufferPool(GeometryBufferPool&&) noexcept            = default;
        GeometryBufferPool& operator=(GeometryBufferPool&&) noexcept = default;

        /**
         * @brief Sub-allocate a range from pooled slabs with specified alignment.
         */
        auto allocate(GPUSize64 size, GPUSize64 alignment = 16) -> BufferSlice;

        /**
         * @brief Release a sub-allocated slice back to the pool free list.
         */
        void free(const BufferSlice& slice);

        /**
         * @brief Immediately destroy or retire all slabs and reset allocations.
         */
        void clear();

        /**
         * @brief Total memory allocated across all GPU buffer slabs in bytes.
         */
        GPUSize64 get_total_allocated() const { return total_capacity; }

        /**
         * @brief Total active bytes sub-allocated across all slices.
         */
        GPUSize64 get_used_bytes() const { return used_bytes; }

        /**
         * @brief Number of active buffer slabs.
         */
        usize get_slab_count() const { return slabs.size(); }

    private:
        struct FreeBlock
        {
            GPUSize64 offset = 0;
            GPUSize64 size   = 0;
        };

        struct Slab
        {
            uint              id = 0;
            GPUBuffer         buffer;
            GPUSize64         capacity        = 0;
            GPUSize64         allocated_bytes = 0;
            Vector<FreeBlock> free_blocks;
            bool              is_dedicated = false;
        };

        auto create_slab(GPUSize64 capacity, bool dedicated = false) -> Slab&;
        void destroy_buffer(GPUBuffer& buffer);

        auto try_allocate_from_slab(Slab& slab, GPUSize64 size, GPUSize64 alignment) -> Optional<BufferSlice>;
        void coalesce_free_blocks(Slab& slab);

    private:
        ResourceRetire& resource_retire;
        GPUSize64       default_slab_size;
        GPUSize64       total_capacity = 0;
        GPUSize64       used_bytes     = 0;
        uint            next_slab_id   = 1;
        Vector<Slab>    slabs;
    };

} // namespace lyra

#endif // LYRA_ENGINE_RENDERING_GEOMETRY_BUFFER_POOL_H
