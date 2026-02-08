#pragma once

#ifndef LYRA_LIBRARY_ENGINE_RENDER_FRAME_GRAPH_ALLOCATOR_H
#define LYRA_LIBRARY_ENGINE_RENDER_FRAME_GRAPH_ALLOCATOR_H

#include "FrameGraphCommon.h" // IWYU pragma: keep

namespace lyra
{
    template <typename T>
    struct FrameGraphAllocatorEntry
    {
        T    data;
        bool used = false;
    };

    using FGBufferObject = GPUBufferHandle;
    using FGBufferVector = Vector<FrameGraphAllocatorEntry<FGBufferObject>>;

    using FGTextureObject = std::pair<GPUTextureHandle, GPUTextureViewHandle>;
    using FGTextureVector = Vector<FrameGraphAllocatorEntry<FGTextureObject>>;

    struct FrameGraphAllocator
    {
    public:
        auto allocate(const GPUBufferDescriptor& descriptor) -> FGBufferObject;
        void recycle(const GPUBufferDescriptor& descriptor, FGBufferObject buffer);

        auto allocate(const GPUTextureDescriptor& descriptor) -> FGTextureObject;
        void recycle(const GPUTextureDescriptor& descriptor, FGTextureObject texture);

    private:
        HashMap<GPUBufferDescriptor, FGBufferVector>   buffers;
        HashMap<GPUTextureDescriptor, FGTextureVector> textures;
    };

} // namespace lyra

#endif // LYRA_LIBRARY_ENGINE_RENDER_FRAME_GRAPH_ALLOCATOR_H
