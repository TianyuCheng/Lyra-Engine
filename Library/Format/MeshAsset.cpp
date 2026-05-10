#include <Lyra/FileIO/VFSAPI.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Format/MeshAsset.h>

using namespace lyra;

static constexpr uint32_t MESH_ASSET_VERSION = 3;
static constexpr uint32_t MESH_MAGIC         = 0x534D594C; // 'LYMS'

struct ChunkHeader
{
    uint32_t type;
    uint32_t size;
};

// Chunk Types
static constexpr uint32_t CHUNK_BBOX = 0x584F4242; // 'BBOX'
static constexpr uint32_t CHUNK_ATTR = 0x52545441; // 'ATTR'
static constexpr uint32_t CHUNK_INDX = 0x58444E49; // 'INDX'
static constexpr uint32_t CHUNK_LODS = 0x53444F4C; // 'LODS'

static void* load_mesh_asset(FileLoader* loader, FSPath path)
{
    auto asset = new MeshAsset();

    auto content = loader->read<uint8_t>(path);
    if (content.empty()) {
        delete asset;
        return nullptr;
    }

    size_t offset   = 0;
    auto   read_raw = [&](void* dest, size_t size) -> bool {
        if (offset + size > content.size()) return false;
        memcpy(dest, content.data() + offset, size);
        offset += size;
        return true;
    };

    // Header: Magic + Version
    uint32_t magic   = 0;
    uint32_t version = 0;
    if (!read_raw(&magic, sizeof(uint32_t)) || magic != MESH_MAGIC) {
        spdlog::error("Failed to load MeshAsset {}: Invalid magic", path);
        delete asset;
        return nullptr;
    }

    if (!read_raw(&version, sizeof(uint32_t)) || version != MESH_ASSET_VERSION) {
        spdlog::error("Failed to load MeshAsset {}: Unsupported version (found {}, expected {})", path, version, MESH_ASSET_VERSION);
        delete asset;
        return nullptr;
    }

    // Chunk-based parsing
    while (offset < content.size()) {
        ChunkHeader chunk;
        if (!read_raw(&chunk, sizeof(ChunkHeader))) break;

        size_t next_chunk_offset = offset + chunk.size;

        if (chunk.type == CHUNK_BBOX) {
            read_raw(&asset->min_bounds, sizeof(Vector3));
            read_raw(&asset->max_bounds, sizeof(Vector3));
        } else if (chunk.type == CHUNK_LODS) {
            uint32_t lod_count = 0;
            read_raw(&lod_count, sizeof(uint32_t));
            asset->lods.resize(lod_count);
            for (uint32_t i = 0; i < lod_count; ++i) {
                auto& lod = asset->lods[i];

                // Read Attributes
                uint32_t attr_count = 0;
                read_raw(&attr_count, sizeof(uint32_t));
                lod.attributes.resize(attr_count);
                for (uint32_t k = 0; k < attr_count; ++k) {
                    auto& attr = lod.attributes[k];
                    read_raw(&attr.semantics, sizeof(MeshSemantics));
                    read_raw(&attr.format, sizeof(GPUVertexFormat));
                    read_raw(&attr.element_count, sizeof(uint32_t));
                    uint32_t data_size = 0;
                    read_raw(&data_size, sizeof(uint32_t));
                    attr.data.resize(data_size);
                    read_raw(attr.data.data(), data_size);
                }

                // Read Indices
                read_raw(&lod.index_format, sizeof(GPUIndexFormat));
                uint32_t index_data_size = 0;
                read_raw(&index_data_size, sizeof(uint32_t));
                lod.index_data.resize(index_data_size);
                read_raw(lod.index_data.data(), index_data_size);

                // Read Surfaces
                uint32_t surface_count = 0;
                read_raw(&surface_count, sizeof(uint32_t));
                lod.surfaces.resize(surface_count);
                for (uint32_t j = 0; j < surface_count; ++j) {
                    auto& surf = lod.surfaces[j];
                    read_raw(&surf.slice, sizeof(GeometrySlice));
                    AssetID material_id;
                    read_raw(&material_id, sizeof(AssetID));
                    surf.material = AssetHandle<MaterialAsset>(material_id);
                    read_raw(&surf.min_bounds, sizeof(Vector3));
                    read_raw(&surf.max_bounds, sizeof(Vector3));
                    read_raw(&surf.first_meshlet, sizeof(uint32_t));
                    read_raw(&surf.meshlet_count, sizeof(uint32_t));
                }
            }
        }

        // Always jump to the next chunk start to skip unknown chunks or padding
        offset = next_chunk_offset;
    }

    return asset;
}

static uint get_mesh_extensions(CString* extensions)
{
    if (extensions) {
        extensions[0] = ".mesh";
    }
    return 1;
}

AssetLoaderAPI MeshAsset::loader()
{
    auto api                     = AssetLoaderAPI{};
    api.configure                = nullptr;
    api.load                     = load_mesh_asset;
    api.unload                   = [](void* asset) { delete reinterpret_cast<MeshAsset*>(asset); };
    api.get_supported_extensions = get_mesh_extensions;
    return api;
}
