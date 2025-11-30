#pragma once

#ifndef LYRA_LIBRARY_FRAME_GRAPH_BUFFER_H
#define LYRA_LIBRARY_FRAME_GRAPH_BUFFER_H

#include <Lyra/Plugin/RHI/RHIDescs.h>
#include <Lyra/Engine/Render/FrameGraphAllocator.h>

namespace lyra
{
    struct FrameGraphBuffer
    {
        using Self       = FrameGraphBuffer;
        using Descriptor = GPUBufferDescriptor;

        void create(FrameGraphAllocator* allocator, const Descriptor& descriptor)
        {
            buffer = allocator->allocate(descriptor);
        }

        void destroy(FrameGraphAllocator* allocator, const Descriptor& descriptor)
        {
            allocator->recycle(descriptor, buffer);
            buffer.reset();
        }

        // related texture handles
        GPUBufferHandle buffer;
    };

} // namespace lyra

#endif // LYRA_LIBRARY_FRAME_GRAPH_BUFFER_H
