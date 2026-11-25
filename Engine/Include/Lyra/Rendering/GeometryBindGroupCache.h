#pragma once

#ifndef LYRA_ENGINE_RENDERING_GEOMETRY_BIND_GROUP_CACHE_H
#define LYRA_ENGINE_RENDERING_GEOMETRY_BIND_GROUP_CACHE_H

#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Graphics/RHIDescs.h>
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Utilities/Pointer.h>

namespace lyra
{
    /**
     * @brief Shared cache for geometry vertex pulling bind groups and heaps across passes.
     */
    class GeometryBindGroupCache
    {
    public:
        GeometryBindGroupCache() = default;
        ~GeometryBindGroupCache() { destroy(); }

        GeometryBindGroupCache(const GeometryBindGroupCache&)            = delete;
        GeometryBindGroupCache& operator=(const GeometryBindGroupCache&) = delete;

        GeometryBindGroupCache(GeometryBindGroupCache&&) noexcept            = default;
        GeometryBindGroupCache& operator=(GeometryBindGroupCache&&) noexcept = default;

        bool init(GPUDevice dev, GPUBindGroupLayout in_layout = {})
        {
            device = dev;
            if (!device.valid()) {
                return false;
            }

            if (in_layout.valid()) {
                layout      = in_layout;
                owns_layout = false;
            } else if (!layout.valid()) {
                GPUBindGroupLayoutEntry entry{};
                entry.binding.index = 0;
                entry.type          = GPUResourceType::BUFFER;
                entry.visibility    = GPUShaderStage::VERTEX;
                entry.buffer.type   = GPUBufferBindingType::READ_ONLY_STORAGE;

                Array<GPUBindGroupLayoutEntry, 1> layout_entries{entry};
                GPUBindGroupLayoutDescriptor      desc{};
                desc.label   = "geometry_vertex_layout";
                desc.entries = layout_entries;
                layout       = device.create_bind_group_layout(desc);
                owns_layout  = true;
            }

            if (!heap.valid()) {
                auto desc      = GPUBindGroupHeapDescriptor{};
                desc.page_size = 512;
                heap           = device.create_bind_group_heap(desc);
            }

            initialized = heap.valid() && layout.valid();
            return initialized;
        }

        void destroy()
        {
            if (device.valid()) {
                if (heap.valid()) {
                    heap.destroy();
                }
                if (owns_layout && layout.valid()) {
                    layout.destroy();
                }
            }
            bind_groups.clear();
            heap        = {};
            layout      = {};
            device      = {};
            owns_layout = false;
            initialized = false;
        }

        auto get_or_create(GPUBufferHandle vb_handle) -> GPUBindGroup
        {
            if (!vb_handle.valid() || !device.valid()) {
                return {};
            }

            auto it = bind_groups.find(vb_handle.value);
            if (it != bind_groups.end()) {
                return it->second;
            }

            if (!layout.valid() || !heap.valid()) {
                return {};
            }

            Array<GPUBindGroupEntry, 1> entries{};
            entries[0].type          = GPUResourceType::BUFFER;
            entries[0].binding       = 0;
            entries[0].buffer.buffer = vb_handle;
            entries[0].buffer.offset = 0;
            entries[0].buffer.size   = 0;

            auto bg_desc    = GPUBindGroupDescriptor{};
            bg_desc.heap    = heap;
            bg_desc.layout  = layout;
            bg_desc.entries = entries;

            auto bg = device.create_bind_group(bg_desc);
            bind_groups.emplace(vb_handle.value, bg);
            return bg;
        }

        void clear()
        {
            bind_groups.clear();
        }

        [[nodiscard]] bool valid() const { return initialized && heap.valid() && layout.valid(); }
        [[nodiscard]] auto get_layout() const -> GPUBindGroupLayoutHandle { return layout; }

    private:
        GPUDevice                       device;
        GPUBindGroupLayout              layout;
        GPUBindGroupHeap                heap;
        HashMap<ulong, GPUBindGroup>    bind_groups;
        bool                            owns_layout = false;
        bool                            initialized = false;
    };

} // namespace lyra

#endif // LYRA_ENGINE_RENDERING_GEOMETRY_BIND_GROUP_CACHE_H
