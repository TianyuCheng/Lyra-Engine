#include <cstring>
#include <algorithm>
#include <Lyra/Graphics/RHIDescs.h>
#include <Lyra/Graphics/RHIInits.h>
#include <Lyra/Graphics/RHIUtils.h>
#include <Lyra/Rendering/ResourceRetire.h>
#include <Lyra/Rendering/GeometryCache.h>

using namespace lyra;

GeometryCache::GeometryCache(ResourceRetire& resource_retire, GPUSize64 slab_size)
    : resource_retire(resource_retire), buffer_pool(resource_retire, slab_size)
{
    // do nothing else
}

GeometryCache::~GeometryCache()
{
    clear();
}

const MeshGeometryEntry* GeometryCache::acquire(
    const MeshAssetHandle& handle,
    const MeshAsset&       asset,
    GeometryForms          forms)
{
    if (!handle.valid() || asset.lods.empty()) {
        return nullptr;
    }

    // 1. check active cache
    auto active_it = active_cache.find(handle);
    if (active_it != active_cache.end()) {
        ++active_it->second.ref_count;
        active_it->second.last_used_frame = current_frame;

        if ((active_it->second.loaded_forms & forms) != forms) {
            upload_forms(active_it->second, asset, forms);
        }
        return &active_it->second;
    }

    // 2. check standby cache (hit without re-allocation)
    auto standby_it = standby_cache.find(handle);
    if (standby_it != standby_cache.end()) {
        MeshGeometryEntry entry = std::move(standby_it->second);
        standby_cache.erase(standby_it);

        entry.ref_count       = 1;
        entry.last_used_frame = current_frame;

        if ((entry.loaded_forms & forms) != forms) {
            upload_forms(entry, asset, forms);
        }

        auto [inserted_it, _] = active_cache.emplace(handle, std::move(entry));
        return &inserted_it->second;
    }

    // 3. new entry
    MeshGeometryEntry entry;
    entry.ref_count       = 1;
    entry.last_used_frame = current_frame;
    entry.local_bounds    = Bounds3D(asset.min_bounds, asset.max_bounds);
    entry.lods.resize(asset.lods.size());

    upload_forms(entry, asset, forms);

    auto [inserted_it, _] = active_cache.emplace(handle, std::move(entry));
    return &inserted_it->second;
}

bool GeometryCache::acquire_meshlet(const MeshAssetHandle& handle, const MeshAsset& asset)
{
    auto it = active_cache.find(handle);
    if (it == active_cache.end()) {
        return false;
    }

    if (it->second.loaded_forms.contains(GeometryForm::MESHLET)) {
        return true;
    }

    upload_forms(it->second, asset, GeometryForm::MESHLET);
    return it->second.loaded_forms.contains(GeometryForm::MESHLET);
}

bool GeometryCache::acquire_ray_tracing(const MeshAssetHandle& handle, const MeshAsset& asset)
{
    auto it = active_cache.find(handle);
    if (it == active_cache.end()) {
        return false;
    }

    if (it->second.loaded_forms.contains(GeometryForm::RAY_TRACING)) {
        return true;
    }

    upload_forms(it->second, asset, GeometryForm::RAY_TRACING);
    return it->second.loaded_forms.contains(GeometryForm::RAY_TRACING);
}

void GeometryCache::release(const MeshAssetHandle& handle)
{
    auto it = active_cache.find(handle);
    if (it == active_cache.end()) {
        return;
    }

    if (it->second.ref_count > 1) {
        --it->second.ref_count;
        return;
    }

    // ref count drops to zero: move to standby LRU cache
    it->second.ref_count       = 0;
    it->second.last_used_frame = current_frame;

    standby_cache.emplace(handle, std::move(it->second));
    active_cache.erase(it);
}

const MeshGeometryEntry* GeometryCache::get(const MeshAssetHandle& handle) const
{
    auto it = active_cache.find(handle);
    if (it != active_cache.end()) {
        return &it->second;
    }

    auto standby_it = standby_cache.find(handle);
    if (standby_it != standby_cache.end()) {
        return &standby_it->second;
    }

    return nullptr;
}

bool GeometryCache::contains(const MeshAssetHandle& handle) const
{
    return active_cache.find(handle) != active_cache.end() ||
           standby_cache.find(handle) != standby_cache.end();
}

void GeometryCache::tick(uint current_frame_idx)
{
    current_frame = current_frame_idx;

    // 1. evict expired standby meshes based on TTL
    for (auto it = standby_cache.begin(); it != standby_cache.end();) {
        if (standby_ttl_frames > 0 && current_frame >= it->second.last_used_frame + standby_ttl_frames) {
            free_entry_geometry(it->second);
            standby_cache.erase(it++);
        } else {
            ++it;
        }
    }

    // 2. evict least recently used standby meshes if VRAM budget exceeded
    if (buffer_pool.get_used_bytes() > vram_budget && !standby_cache.empty()) {
        Vector<std::pair<MeshAssetHandle, uint>> lru_candidates;
        lru_candidates.reserve(standby_cache.size());

        for (const auto& [handle, entry] : standby_cache) {
            lru_candidates.emplace_back(handle, entry.last_used_frame);
        }

        std::sort(lru_candidates.begin(), lru_candidates.end(), [](const auto& a, const auto& b) {
            return a.second < b.second; // oldest frame first
        });

        for (const auto& [handle, _] : lru_candidates) {
            if (buffer_pool.get_used_bytes() <= vram_budget) {
                break;
            }

            auto it = standby_cache.find(handle);
            if (it != standby_cache.end()) {
                free_entry_geometry(it->second);
                standby_cache.erase(it);
            }
        }
    }
}

void GeometryCache::clear()
{
    for (auto& [_, entry] : active_cache) {
        free_entry_geometry(entry);
    }
    active_cache.clear();

    for (auto& [_, entry] : standby_cache) {
        free_entry_geometry(entry);
    }
    standby_cache.clear();

    buffer_pool.clear();
}

usize GeometryCache::total_references() const
{
    usize total = 0;
    for (const auto& [_, entry] : active_cache) {
        total += entry.ref_count;
    }
    return total;
}

void GeometryCache::upload_forms(
    MeshGeometryEntry& entry,
    const MeshAsset&   asset,
    GeometryForms      requested)
{
    Vector<GeometryCopy> copy_ops;
    Vector<uint8_t>      cpu_staging;

    if (entry.lods.size() < asset.lods.size()) {
        entry.lods.resize(asset.lods.size());
    }

    for (uint i = 0; i < asset.lods.size(); ++i) {
        auto&       gpu_lod = entry.lods[i];
        const auto& lod     = asset.lods[i];
        gpu_lod.surfaces    = lod.surfaces;

        // 1. Indexed Presentation
        if (requested.contains(GeometryForm::INDEXED) && !gpu_lod.indexed.has_value()) {
            upload_indexed_lod(gpu_lod, lod, copy_ops, cpu_staging);
        }

        // 2. Meshlet Presentation
        if (requested.contains(GeometryForm::MESHLET) && !gpu_lod.meshlet.has_value()) {
            upload_meshlet_lod(gpu_lod, lod, copy_ops, cpu_staging);
        }

        // 3. Ray Tracing (BLAS) Presentation
        if (requested.contains(GeometryForm::RAY_TRACING) && !gpu_lod.ray_tracing.has_value()) {
            if (!gpu_lod.indexed.has_value()) {
                upload_indexed_lod(gpu_lod, lod, copy_ops, cpu_staging);
            }
            upload_ray_tracing_lod(gpu_lod, lod);
        }
    }

    // execute staging upload to device-local VRAM
    if (RHI::api() && !cpu_staging.empty() && !copy_ops.empty()) {
        auto& device = RHI::get_current_device();

        auto staging_desc               = GPUBufferDescriptor{};
        staging_desc.label              = "geometry_staging_upload";
        staging_desc.size               = cpu_staging.size();
        staging_desc.usage              = GPUBufferUsage::MAP_WRITE | GPUBufferUsage::COPY_SRC;
        staging_desc.mapped_at_creation = true;

        auto staging = device.create_buffer(staging_desc);
        if (staging.valid()) {
            auto mapped = staging.get_mapped_range();
            if (mapped.data) {
                std::memcpy(mapped.data, cpu_staging.data(), cpu_staging.size());
            }
            staging.unmap();

            auto cmd_desc  = GPUCommandBufferDescriptor{};
            cmd_desc.label = "geometry_upload_copy_cmd";
            auto cmd       = device.create_command_buffer(cmd_desc);

            for (const auto& op : copy_ops) {
                if (op.dst_buffer.valid() && op.size > 0) {
                    GPUBuffer dst_buf;
                    dst_buf.handle = op.dst_buffer;
                    cmd.copy_buffer_to_buffer(staging, op.src_offset, dst_buf, op.dst_offset, op.size);
                }
            }

            cmd.submit();
            device.wait();
            staging.destroy();
        }
    }

    entry.loaded_forms |= requested;
}

void GeometryCache::upload_indexed_lod(
    GeometryLOD&          gpu_lod,
    const MeshLOD&        lod,
    Vector<GeometryCopy>& copy_ops,
    Vector<uint8_t>&      cpu_staging)
{
    IndexedGeometry geom;
    geom.index_format = lod.index_format;

    // 1. Identify present attribute streams
    const MeshAttribute* pos_attr       = nullptr;
    const MeshAttribute* norm_attr      = nullptr;
    const MeshAttribute* tangent_attr   = nullptr;
    const MeshAttribute* bitangent_attr = nullptr;
    const MeshAttribute* uv0_attr       = nullptr;
    const MeshAttribute* uv1_attr       = nullptr;
    const MeshAttribute* color0_attr    = nullptr;
    const MeshAttribute* joints0_attr   = nullptr;
    const MeshAttribute* weights0_attr  = nullptr;
    const MeshAttribute* joints1_attr   = nullptr;
    const MeshAttribute* weights1_attr  = nullptr;

    for (const auto& attr : lod.attributes) {
        if (attr.element_count == 0 || attr.data.empty()) {
            continue;
        }
        // clang-format off
        switch (attr.semantics) {
            case MeshSemantics::POSITION:   pos_attr       = &attr; break;
            case MeshSemantics::NORMAL:     norm_attr      = &attr; break;
            case MeshSemantics::TANGENT:    tangent_attr   = &attr; break;
            case MeshSemantics::BITANGENT:  bitangent_attr = &attr; break;
            case MeshSemantics::TEXCOORD0:  uv0_attr       = &attr; break;
            case MeshSemantics::TEXCOORD1:  uv1_attr       = &attr; break;
            case MeshSemantics::COLOR0:     color0_attr    = &attr; break;
            case MeshSemantics::JOINTS0:    joints0_attr   = &attr; break;
            case MeshSemantics::JOINTS1:    joints1_attr   = &attr; break;
            case MeshSemantics::WEIGHTS0:   weights0_attr  = &attr; break;
            case MeshSemantics::WEIGHTS1:   weights1_attr  = &attr; break;
            default:
                break;
        }
        // clang-format on
    }

    uint vertex_count = pos_attr ? pos_attr->element_count : 0;
    if (vertex_count == 0) {
        for (const auto& attr : lod.attributes) {
            if (attr.element_count > 0) {
                vertex_count = std::max(vertex_count, attr.element_count);
            }
        }
    }
    geom.vertex_count = vertex_count;

    auto align_up = [](GPUSize64 val, GPUSize64 align) -> GPUSize64 {
        return (val + align - 1) & ~(align - 1);
    };

    // 2. Compute stream offsets and total allocation size for represented attributes
    VertexStreamOffsets stream_offsets;
    GPUSize64           current_offset = 0;

    if (pos_attr && !pos_attr->data.empty()) {
        stream_offsets.position = static_cast<uint>(current_offset);
        current_offset += align_up(pos_attr->data.size(), 16);
    }
    if (norm_attr && !norm_attr->data.empty()) {
        stream_offsets.normal = static_cast<uint>(current_offset);
        current_offset += align_up(norm_attr->data.size(), 16);
    }
    if (tangent_attr && !tangent_attr->data.empty()) {
        stream_offsets.tangent = static_cast<uint>(current_offset);
        current_offset += align_up(tangent_attr->data.size(), 16);
    }
    if (bitangent_attr && !bitangent_attr->data.empty()) {
        stream_offsets.bitangent = static_cast<uint>(current_offset);
        current_offset += align_up(bitangent_attr->data.size(), 16);
    }
    if (uv0_attr && !uv0_attr->data.empty()) {
        stream_offsets.texcoord0 = static_cast<uint>(current_offset);
        current_offset += align_up(uv0_attr->data.size(), 16);
    }
    if (uv1_attr && !uv1_attr->data.empty()) {
        stream_offsets.texcoord1 = static_cast<uint>(current_offset);
        current_offset += align_up(uv1_attr->data.size(), 16);
    }
    if (color0_attr && !color0_attr->data.empty()) {
        stream_offsets.color0 = static_cast<uint>(current_offset);
        current_offset += align_up(color0_attr->data.size(), 16);
    }
    if (joints0_attr && !joints0_attr->data.empty()) {
        stream_offsets.joints0 = static_cast<uint>(current_offset);
        current_offset += align_up(joints0_attr->data.size(), 16);
    }
    if (weights0_attr && !weights0_attr->data.empty()) {
        stream_offsets.weights0 = static_cast<uint>(current_offset);
        current_offset += align_up(weights0_attr->data.size(), 16);
    }
    if (joints1_attr && !joints1_attr->data.empty()) {
        stream_offsets.joints1 = static_cast<uint>(current_offset);
        current_offset += align_up(joints1_attr->data.size(), 16);
    }
    if (weights1_attr && !weights1_attr->data.empty()) {
        stream_offsets.weights1 = static_cast<uint>(current_offset);
        current_offset += align_up(weights1_attr->data.size(), 16);
    }

    geom.stream_offsets = stream_offsets;

    if (vertex_count > 0 && current_offset > 0) {
        GPUSize64 total_vb_size = current_offset;
        geom.vertex_slice       = buffer_pool.allocate(total_vb_size, 16);

        GPUSize64 vb_staging_offset = cpu_staging.size();
        cpu_staging.resize(vb_staging_offset + total_vb_size, 0);
        uint8_t* dst = cpu_staging.data() + vb_staging_offset;

        if (pos_attr && !pos_attr->data.empty()) {
            std::memcpy(dst + stream_offsets.position, pos_attr->data.data(), pos_attr->data.size());
        }
        if (norm_attr && !norm_attr->data.empty()) {
            std::memcpy(dst + stream_offsets.normal, norm_attr->data.data(), norm_attr->data.size());
        }
        if (tangent_attr && !tangent_attr->data.empty()) {
            std::memcpy(dst + stream_offsets.tangent, tangent_attr->data.data(), tangent_attr->data.size());
        }
        if (bitangent_attr && !bitangent_attr->data.empty()) {
            std::memcpy(dst + stream_offsets.bitangent, bitangent_attr->data.data(), bitangent_attr->data.size());
        }
        if (uv0_attr && !uv0_attr->data.empty()) {
            std::memcpy(dst + stream_offsets.texcoord0, uv0_attr->data.data(), uv0_attr->data.size());
        }
        if (uv1_attr && !uv1_attr->data.empty()) {
            std::memcpy(dst + stream_offsets.texcoord1, uv1_attr->data.data(), uv1_attr->data.size());
        }
        if (color0_attr && !color0_attr->data.empty()) {
            std::memcpy(dst + stream_offsets.color0, color0_attr->data.data(), color0_attr->data.size());
        }
        if (joints0_attr && !joints0_attr->data.empty()) {
            std::memcpy(dst + stream_offsets.joints0, joints0_attr->data.data(), joints0_attr->data.size());
        }
        if (weights0_attr && !weights0_attr->data.empty()) {
            std::memcpy(dst + stream_offsets.weights0, weights0_attr->data.data(), weights0_attr->data.size());
        }
        if (joints1_attr && !joints1_attr->data.empty()) {
            std::memcpy(dst + stream_offsets.joints1, joints1_attr->data.data(), joints1_attr->data.size());
        }
        if (weights1_attr && !weights1_attr->data.empty()) {
            std::memcpy(dst + stream_offsets.weights1, weights1_attr->data.data(), weights1_attr->data.size());
        }

        // position_slice aliases the continuous position chunk
        if (stream_offsets.has_position() && pos_attr) {
            geom.position_slice.buffer  = geom.vertex_slice.buffer;
            geom.position_slice.offset  = geom.vertex_slice.offset + stream_offsets.position;
            geom.position_slice.size    = pos_attr->data.size();
            geom.position_slice.slab_id = geom.vertex_slice.slab_id;
        }

        GeometryCopy op;
        op.src_offset = vb_staging_offset;
        op.dst_buffer = geom.vertex_slice.buffer;
        op.dst_offset = geom.vertex_slice.offset;
        op.size       = total_vb_size;
        copy_ops.push_back(op);
    }

    // 2. Upload index buffer
    if (!lod.index_data.empty()) {
        uint index_stride = get_index_format_size(lod.index_format);
        geom.index_count  = (index_stride > 0) ? static_cast<uint>(lod.index_data.size() / index_stride) : 0;
        geom.index_slice  = buffer_pool.allocate(lod.index_data.size(), 4);

        GPUSize64 ib_staging_offset = cpu_staging.size();
        cpu_staging.insert(cpu_staging.end(), lod.index_data.begin(), lod.index_data.end());

        GeometryCopy op;
        op.src_offset = ib_staging_offset;
        op.dst_buffer = geom.index_slice.buffer;
        op.dst_offset = geom.index_slice.offset;
        op.size       = lod.index_data.size();
        copy_ops.push_back(op);
    }

    gpu_lod.indexed = std::move(geom);
}

void GeometryCache::upload_ray_tracing_lod(GeometryLOD& gpu_lod, const MeshLOD& lod)
{
    if (!RHI::api()) {
        return;
    }

    auto& device = RHI::get_current_device();
    if (!device.features.raytracing) {
        return;
    }

    uint vertex_count = 0;
    for (const auto& attr : lod.attributes) {
        if (attr.element_count > 0) {
            vertex_count = std::max(vertex_count, attr.element_count);
        }
    }

    uint index_stride = get_index_format_size(lod.index_format);
    uint index_count  = (index_stride > 0) ? static_cast<uint>(lod.index_data.size() / index_stride) : 0;

    if (vertex_count == 0 || index_count == 0) {
        return;
    }

    auto desc  = GPUBlasDescriptor{};
    desc.label = "mesh_geometry_blas";
    desc.flags = GPUBVHFlag::PREFER_FAST_TRACE;

    auto size                    = GPUBlasGeometrySizeDescriptor{};
    size.type                    = GPUBlasType::TRIANGLE;
    size.triangles.flags         = GPUBVHGeometryFlag::BVH_OPAQUE;
    size.triangles.vertex_format = GPUVertexFormat::FLOAT32x3;
    size.triangles.vertex_count  = vertex_count;
    size.triangles.index_count   = index_count;
    size.triangles.index_format  = lod.index_format;

    Vector<GPUBlasGeometrySizeDescriptor> sizes = {size};

    RayTracingGeometry rt_geom;
    rt_geom.blas     = device.create_blas(desc, sizes);
    rt_geom.is_built = false;

    gpu_lod.ray_tracing = std::move(rt_geom);
}

void GeometryCache::upload_meshlet_lod(
    GeometryLOD&   gpu_lod,
    const MeshLOD& lod,
    Vector<GeometryCopy>& /*copy_ops*/,
    Vector<uint8_t>& /*cpu_staging*/
)
{
    MeshletGeometry meshlet_geom;
    uint            total_meshlets = 0;

    for (const auto& surf : lod.surfaces) {
        total_meshlets += surf.meshlet_count;
    }
    meshlet_geom.meshlet_count = total_meshlets;

    if (total_meshlets > 0) {
        GPUSize64 desc_size        = sizeof(MeshletDesc) * total_meshlets;
        meshlet_geom.meshlet_slice = buffer_pool.allocate(desc_size, 64);
    }

    gpu_lod.meshlet = std::move(meshlet_geom);
}

void GeometryCache::free_entry_geometry(MeshGeometryEntry& entry)
{
    for (auto& lod : entry.lods) {
        if (lod.indexed.has_value()) {
            buffer_pool.free(lod.indexed->vertex_slice);
            if (lod.indexed->position_slice.valid() && lod.indexed->position_slice.buffer != lod.indexed->vertex_slice.buffer) {
                buffer_pool.free(lod.indexed->position_slice);
            }
            buffer_pool.free(lod.indexed->index_slice);
            lod.indexed.reset();
        }

        if (lod.meshlet.has_value()) {
            buffer_pool.free(lod.meshlet->meshlet_slice);
            buffer_pool.free(lod.meshlet->meshlet_vertex_slice);
            buffer_pool.free(lod.meshlet->meshlet_triangle_slice);
            buffer_pool.free(lod.meshlet->vertex_storage_slice);
            lod.meshlet.reset();
        }

        if (lod.ray_tracing.has_value()) {
            if (lod.ray_tracing->blas.valid()) {
                resource_retire.retire(lod.ray_tracing->blas);
            }
            lod.ray_tracing.reset();
        }
    }
}
