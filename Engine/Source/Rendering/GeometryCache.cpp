#include <cstring>
#include <Lyra/Graphics/RHIDescs.h>
#include <Lyra/Graphics/RHIInits.h>
#include <Lyra/Graphics/RHIUtils.h>
#include <Lyra/Runtime/RenderLayer.h>
#include <Lyra/Rendering/GeometryCache.h>

using namespace lyra;

GeometryCache::GeometryCache(RenderLayer* render_layer)
    : render_layer(render_layer)
{
}

GeometryCache::~GeometryCache()
{
    clear();
}

void GeometryCache::set_render_layer(RenderLayer* layer)
{
    render_layer = layer;
}

const GPUMeshEntry* GeometryCache::acquire(const MeshAssetHandle& handle, const MeshAsset& asset)
{
    if (!handle.valid() || asset.lods.empty()) {
        return nullptr;
    }

    auto it = cache.find(handle);
    if (it != cache.end()) {
        ++it->second.ref_count;
        return &it->second;
    }

    // upload new entry
    GPUMeshEntry entry;
    entry.ref_count    = 1;
    entry.local_bounds = Bounds3D(asset.min_bounds, asset.max_bounds);
    entry.lods.reserve(asset.lods.size());

    for (uint i = 0; i < asset.lods.size(); ++i) {
        entry.lods.push_back(upload_lod(asset.lods[i], i));
    }

    auto [inserted_it, _] = cache.emplace(handle, std::move(entry));
    return &inserted_it->second;
}

void GeometryCache::release(const MeshAssetHandle& handle)
{
    auto it = cache.find(handle);
    if (it == cache.end()) {
        return;
    }

    if (it->second.ref_count > 1) {
        --it->second.ref_count;
        return;
    }

    // ref count reached 0: destroy/enqueue buffers
    for (auto& lod : it->second.lods) {
        if (render_layer) {
            if (lod.vertex_buffer.valid()) {
                render_layer->enqueue(lod.vertex_buffer);
            }
            if (lod.index_buffer.valid()) {
                render_layer->enqueue(lod.index_buffer);
            }
        } else {
            if (lod.vertex_buffer.valid()) {
                lod.vertex_buffer.destroy();
            }
            if (lod.index_buffer.valid()) {
                lod.index_buffer.destroy();
            }
        }
    }

    cache.erase(it);
}

const GPUMeshEntry* GeometryCache::get(const MeshAssetHandle& handle) const
{
    auto it = cache.find(handle);
    if (it != cache.end()) {
        return &it->second;
    }
    return nullptr;
}

bool GeometryCache::contains(const MeshAssetHandle& handle) const
{
    return cache.find(handle) != cache.end();
}

void GeometryCache::clear()
{
    for (auto& [_, entry] : cache) {
        for (auto& lod : entry.lods) {
            if (render_layer) {
                if (lod.vertex_buffer.valid()) {
                    render_layer->enqueue(lod.vertex_buffer);
                }
                if (lod.index_buffer.valid()) {
                    render_layer->enqueue(lod.index_buffer);
                }
            } else {
                if (lod.vertex_buffer.valid()) {
                    lod.vertex_buffer.destroy();
                }
                if (lod.index_buffer.valid()) {
                    lod.index_buffer.destroy();
                }
            }
        }
    }
    cache.clear();
}

usize GeometryCache::total_references() const
{
    usize total = 0;
    for (const auto& [_, entry] : cache) {
        total += entry.ref_count;
    }
    return total;
}

GPUMeshLOD GeometryCache::upload_lod(const MeshLOD& lod, uint lod_index)
{
    GPUMeshLOD gpu_lod;
    gpu_lod.index_format = lod.index_format;
    gpu_lod.surfaces     = lod.surfaces;

    auto& device = RHI::get_current_device();

    // 1. Pack / Interleave vertex attributes
    uint vertex_count = 0;
    for (const auto& attr : lod.attributes) {
        if (attr.element_count > 0) {
            vertex_count = std::max(vertex_count, attr.element_count);
        }
    }
    gpu_lod.vertex_count = vertex_count;

    if (vertex_count > 0 && !lod.attributes.empty()) {
        uint stride = 0;
        for (const auto& attr : lod.attributes) {
            stride += get_vertex_format_size(attr.format);
        }

        if (stride > 0) {
            GPUSize64 total_vb_size = static_cast<GPUSize64>(vertex_count) * stride;

            auto desc               = GPUBufferDescriptor{};
            desc.label              = "geometry_vertex_buffer";
            desc.size               = total_vb_size;
            desc.usage              = GPUBufferUsage::VERTEX | GPUBufferUsage::MAP_WRITE;
            desc.mapped_at_creation = true;

            gpu_lod.vertex_buffer = device.create_buffer(desc);

            auto mapped = gpu_lod.vertex_buffer.get_mapped_range();
            auto dst    = static_cast<uint8_t*>(mapped.data);

            if (dst) {
                // Interleave attributes into mapped memory
                uint current_offset = 0;
                for (const auto& attr : lod.attributes) {
                    uint           attr_elem_size = get_vertex_format_size(attr.format);
                    const uint8_t* src            = attr.data.data();

                    for (uint v = 0; v < vertex_count; ++v) {
                        uint8_t* dst_elem = dst + (v * stride) + current_offset;
                        if (v < attr.element_count && src) {
                            std::memcpy(dst_elem, src + (v * attr_elem_size), attr_elem_size);
                        } else {
                            std::memset(dst_elem, 0, attr_elem_size);
                        }
                    }
                    current_offset += attr_elem_size;
                }
            }

            gpu_lod.vertex_buffer.unmap();
        }
    }

    // 2. Upload index buffer
    if (!lod.index_data.empty()) {
        uint index_stride   = get_index_format_size(lod.index_format);
        gpu_lod.index_count = (index_stride > 0) ? static_cast<uint>(lod.index_data.size() / index_stride) : 0;

        auto desc               = GPUBufferDescriptor{};
        desc.label              = "geometry_index_buffer";
        desc.size               = lod.index_data.size();
        desc.usage              = GPUBufferUsage::INDEX | GPUBufferUsage::MAP_WRITE;
        desc.mapped_at_creation = true;

        gpu_lod.index_buffer = device.create_buffer(desc);

        auto mapped = gpu_lod.index_buffer.get_mapped_range();
        if (mapped.data) {
            std::memcpy(mapped.data, lod.index_data.data(), lod.index_data.size());
        }

        gpu_lod.index_buffer.unmap();
    }

    return gpu_lod;
}
