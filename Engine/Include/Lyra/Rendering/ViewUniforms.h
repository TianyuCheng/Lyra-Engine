#pragma once

#ifndef LYRA_ENGINE_RENDERING_VIEW_UNIFORMS_H
#define LYRA_ENGINE_RENDERING_VIEW_UNIFORMS_H

#include <Lyra/Utilities/Collections.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Rendering/FrameArena.h>
#include <Lyra/Rendering/RenderView.h>
#include <Lyra/Rendering/ShadingBuffers.h>

namespace lyra
{
    /**
     * @brief Manages camera/view uniform staging and bind group deduplication for a frame.
     */
    struct ViewUniforms
    {
        GPUBufferSlice                  slice{};
        HashMap<uint64_t, GPUBindGroup> bind_groups{};

        /// Stages camera uniforms into the frame arena.
        void init(const RenderView& view, FrameArena& arena, GPUDevice device = {})
        {
            slice = arena.create_uniform(CameraUniforms::from_view(view), 256, device);
            bind_groups.clear();
        }

        /// Stages raw CameraUniforms into the frame arena.
        void init(const CameraUniforms& uniforms, FrameArena& arena, GPUDevice device = {})
        {
            slice = arena.create_uniform(uniforms, 256, device);
            bind_groups.clear();
        }

        /// Retrieves or creates a cached bind group for the given camera layout.
        auto bind_group(GPUBindGroupLayout layout, FrameArena& arena, GPUDevice device = {}) -> GPUBindGroup
        {
            if (!layout.handle.valid() || !slice.valid()) {
                return {};
            }

            auto it = bind_groups.find(layout.handle.value);
            if (it != bind_groups.end() && it->second.handle.valid()) {
                return it->second;
            }

            Array<GPUBindGroupEntry, 1> entries{};
            entries[0].type    = GPUResourceType::BUFFER;
            entries[0].binding = 0;
            entries[0].buffer  = slice;

            GPUBindGroupDescriptor desc{};
            desc.layout  = layout;
            desc.entries = entries;

            auto bg                          = arena.allocate(desc, device);
            bind_groups[layout.handle.value] = bg;
            return bg;
        }

        void reset()
        {
            slice = {};
            bind_groups.clear();
        }
    };
} // namespace lyra

#endif // LYRA_ENGINE_RENDERING_VIEW_UNIFORMS_H
