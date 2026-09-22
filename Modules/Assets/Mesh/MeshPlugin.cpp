#include <fstream>
#include <Lyra/Common/Macros.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/FileIO/VFSAPI.h>
#include <Lyra/Assets/Format/MeshAsset.h>

using namespace lyra;

static Logger get_logger()
{
    static Logger logger = create_logger("Mesh", LogLevel::trace);
    return logger;
}

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
        get_logger()->error("failed to load MeshAsset {}: invalid magic", path);
        delete asset;
        return nullptr;
    }

    if (!reader.read(version) || version != MeshAsset::MESH_ASSET_VERSION) {
        get_logger()->error("failed to load MeshAsset {}: unsupported version (found {}, expected {})", path, version, MeshAsset::MESH_ASSET_VERSION);
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
            get_logger()->error("failed to load MeshAsset {}: error reading chunk 0x{:08X}", path, chunk.type);
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

static bool save_mesh_asset(const void* raw_asset, OSPath path)
{
    const auto* asset = reinterpret_cast<const MeshAsset*>(raw_asset);
    if (!asset) return false;

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    auto write_raw = [&](const void* data, size_t size) {
        file.write(reinterpret_cast<const char*>(data), size);
    };

    auto write_uint = [&](uint data) {
        write_raw(&data, sizeof(uint));
    };

    auto write_chunk = [&](uint type, auto&& write_body) {
        ChunkHeader header;
        header.type = type;

        auto start_pos = file.tellp();
        write_raw(&header, sizeof(header));
        auto body_start = file.tellp();

        write_body();

        auto body_end = file.tellp();
        header.size   = static_cast<uint>(body_end - body_start);

        file.seekp(start_pos);
        write_raw(&header, sizeof(header));
        file.seekp(body_end);
    };

    // header: magic + version
    write_uint(MeshAsset::MESH_MAGIC);
    write_uint(MeshAsset::MESH_ASSET_VERSION);

    // bbox chunk
    write_chunk(MeshAsset::CHUNK_BBOX, [&]() {
        write_raw(&asset->min_bounds, sizeof(asset->min_bounds));
        write_raw(&asset->max_bounds, sizeof(asset->max_bounds));
    });

    // lods chunk
    write_chunk(MeshAsset::CHUNK_LODS, [&]() {
        write_uint(static_cast<uint>(asset->lods.size()));
        for (const auto& lod : asset->lods) {
            // attributes
            write_uint(static_cast<uint>(lod.attributes.size()));
            for (const auto& attr : lod.attributes) {
                write_raw(&attr.semantics, sizeof(attr.semantics));
                write_raw(&attr.format, sizeof(attr.format));
                write_uint(attr.element_count);
                write_uint(static_cast<uint>(attr.data.size()));
                write_raw(attr.data.data(), attr.data.size());
            }

            // indices
            write_raw(&lod.index_format, sizeof(lod.index_format));
            write_uint(static_cast<uint>(lod.index_data.size()));
            write_raw(lod.index_data.data(), lod.index_data.size());

            // surfaces
            write_uint(static_cast<uint>(lod.surfaces.size()));
            for (const auto& surf : lod.surfaces) {
                write_raw(&surf.slice, sizeof(surf.slice));
                write_raw(&surf.material.guid, sizeof(surf.material.guid));
                write_raw(&surf.min_bounds, sizeof(surf.min_bounds));
                write_raw(&surf.max_bounds, sizeof(surf.max_bounds));
                write_uint(surf.first_meshlet);
                write_uint(surf.meshlet_count);
            }
        }
    });

    file.close();
    return !file.fail();
}

namespace lyra::mesh::loader
{
    void prepare()
    {
        get_logger()->set_level(parse_log_level_from_env("LYRA_MESH_VERBOSITY"));
    }

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

namespace lyra::mesh::saver
{
    void prepare() {}
    void cleanup() {}

    auto create() -> AssetSaverAPI
    {
        auto api                     = AssetSaverAPI{};
        api.configure                = nullptr;
        api.save                     = save_mesh_asset;
        api.get_supported_extensions = get_mesh_extensions;
        return api;
    }
} // namespace lyra::mesh::saver
