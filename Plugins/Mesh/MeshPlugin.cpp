#include <Lyra/Common/Macros.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/FileIO/VFSAPI.h>
#include <Lyra/Format/MeshAsset.h>

using namespace lyra;

struct ChunkHeader
{
    uint type;
    uint size;
};

struct MeshReader
{
    const Vector<uint8_t>& content;
    size_t&                offset;

    bool read_raw(void* dest, size_t size)
    {
        if (offset + size > content.size()) return false;
        memcpy(dest, content.data() + offset, size);
        offset += size;
        return true;
    }

    template <typename T>
    bool read(T& dest)
    {
        return read_raw(&dest, sizeof(T));
    }
};

static bool read_bbox_chunk(MeshReader& reader, MeshAsset* asset)
{
    if (!reader.read(asset->min_bounds)) return false;
    if (!reader.read(asset->max_bounds)) return false;
    return true;
}

static bool read_mesh_attribute(MeshReader& reader, MeshAttribute& attr)
{
    if (!reader.read(attr.semantics)) return false;
    if (!reader.read(attr.format)) return false;
    if (!reader.read(attr.element_count)) return false;

    uint data_size = 0;
    if (!reader.read(data_size)) return false;

    attr.data.resize(data_size);
    if (!reader.read_raw(attr.data.data(), data_size)) return false;

    return true;
}

static bool read_mesh_surface(MeshReader& reader, MeshSurface& surf)
{
    if (!reader.read(surf.slice)) return false;

    AssetID material_id;
    if (!reader.read(material_id)) return false;
    surf.material = AssetHandle<MaterialAsset>(material_id);

    if (!reader.read(surf.min_bounds)) return false;
    if (!reader.read(surf.max_bounds)) return false;
    if (!reader.read(surf.first_meshlet)) return false;
    if (!reader.read(surf.meshlet_count)) return false;

    return true;
}

static bool read_mesh_lod(MeshReader& reader, MeshLOD& lod)
{
    // read attributes
    uint attr_count = 0;
    if (!reader.read(attr_count)) return false;
    lod.attributes.resize(attr_count);
    for (uint i = 0; i < attr_count; ++i) {
        if (!read_mesh_attribute(reader, lod.attributes[i])) return false;
    }

    // read indices
    if (!reader.read(lod.index_format)) return false;
    uint index_data_size = 0;
    if (!reader.read(index_data_size)) return false;
    lod.index_data.resize(index_data_size);
    if (!reader.read_raw(lod.index_data.data(), index_data_size)) return false;

    // read surfaces
    uint surface_count = 0;
    if (!reader.read(surface_count)) return false;
    lod.surfaces.resize(surface_count);
    for (uint i = 0; i < surface_count; ++i) {
        if (!read_mesh_surface(reader, lod.surfaces[i])) return false;
    }

    return true;
}

static bool read_lods_chunk(MeshReader& reader, MeshAsset* asset)
{
    uint lod_count = 0;
    if (!reader.read(lod_count)) return false;
    asset->lods.resize(lod_count);
    for (uint i = 0; i < lod_count; ++i) {
        if (!read_mesh_lod(reader, asset->lods[i])) return false;
    }
    return true;
}

static void* load_mesh_asset(FileLoader* loader, FSPath path)
{
    auto asset = new MeshAsset();

    auto content = loader->read<uint8_t>(path);
    if (content.empty()) {
        delete asset;
        return nullptr;
    }

    size_t     offset = 0;
    MeshReader reader{content, offset};

    // header: magic + version
    uint magic   = 0;
    uint version = 0;

    if (!reader.read(magic) || magic != MeshAsset::MESH_MAGIC) {
        spdlog::error("Failed to load MeshAsset {}: Invalid magic", path);
        delete asset;
        return nullptr;
    }

    if (!reader.read(version) || version != MeshAsset::MESH_ASSET_VERSION) {
        spdlog::error("Failed to load MeshAsset {}: Unsupported version (found {}, expected {})", path, version, MeshAsset::MESH_ASSET_VERSION);
        delete asset;
        return nullptr;
    }

    // chunk-based parsing
    while (offset < content.size()) {
        ChunkHeader chunk;
        if (!reader.read(chunk)) break;

        size_t next_chunk_offset = offset + chunk.size;
        bool   success           = true;

        if (chunk.type == MeshAsset::CHUNK_BBOX) {
            success = read_bbox_chunk(reader, asset);
        } else if (chunk.type == MeshAsset::CHUNK_LODS) {
            success = read_lods_chunk(reader, asset);
        }

        if (!success) {
            spdlog::error("Failed to load MeshAsset {}: Error reading chunk 0x{:08X}", path, chunk.type);
            delete asset;
            return nullptr;
        }

        // always jump to the next chunk start to skip unknown chunks or padding
        offset = next_chunk_offset;
    }

    return asset;
}

static void unload_mesh_asset(void* asset)
{
    delete reinterpret_cast<MeshAsset*>(asset);
}

static uint get_mesh_extensions(CString* extensions)
{
    if (extensions) {
        extensions[0] = ".mesh";
    }
    return 1;
}

namespace lyra::mesh::loader
{
    void prepare() {}

    void cleanup() {}

    auto create() -> AssetLoaderAPI
    {
        auto api                     = AssetLoaderAPI{};
        api.configure                = nullptr;
        api.load                     = load_mesh_asset;
        api.unload                   = unload_mesh_asset;
        api.get_supported_extensions = get_mesh_extensions;
        return api;
    }
} // namespace lyra::mesh::loader
