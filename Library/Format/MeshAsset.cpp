#include <fstream>

#include <Lyra/Common/Macros.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/FileIO/VFSAPI.h>
#include <Lyra/Format/MeshAsset.h>

using namespace lyra;

// forward declarations for inlined plugins
namespace lyra::mesh::loader
{
    extern AssetLoaderAPI create();
    extern void           prepare();
    extern void           cleanup();
} // namespace lyra::mesh::loader

using MeshLoaderPlugin = BuiltinPlugin<AssetLoaderAPI>;

AssetLoaderAPI MeshAsset::loader()
{
    static Own<MeshLoaderPlugin> PLUGIN;
    if (!PLUGIN)
        PLUGIN = std::make_unique<MeshLoaderPlugin>(
            lyra::mesh::loader::create,
            lyra::mesh::loader::prepare,
            lyra::mesh::loader::cleanup);
    return *PLUGIN->get_api();
}

bool MeshAsset::save(OSPath path) const
{
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    auto write_raw = [&](const void* data, size_t size) {
        file.write(reinterpret_cast<const char*>(data), size);
    };

    auto write_uint = [&](uint data) {
        write_raw(&data, sizeof(uint));
    };

    struct ChunkHeader
    {
        uint type;
        uint size;
    };

    auto write_chunk = [&](uint type, auto&& write_body) {
        ChunkHeader header;
        header.type = type;

        // record position to fill size later
        auto start_pos = file.tellp();
        write_raw(&header, sizeof(header)); // placeholder header
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

    // BBOX chunk
    write_chunk(MeshAsset::CHUNK_BBOX, [&]() {
        write_raw(&min_bounds, sizeof(min_bounds));
        write_raw(&max_bounds, sizeof(max_bounds));
    });

    // LODS chunk
    write_chunk(MeshAsset::CHUNK_LODS, [&]() {
        write_uint(static_cast<uint>(lods.size()));
        for (const auto& lod : lods) {
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
                write_raw(&surf.material.uuid, sizeof(surf.material.uuid));
                write_raw(&surf.min_bounds, sizeof(surf.min_bounds));
                write_raw(&surf.max_bounds, sizeof(surf.max_bounds));
                write_uint(surf.first_meshlet);
                write_uint(surf.meshlet_count);
            }
        }
    });

    return true;
}
