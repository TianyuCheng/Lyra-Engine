#include <Lyra/FileIO/VFSAPI.h>
#include <Lyra/Common/Logger.h>

#include <Lyra/Assets/Types/MeshAsset.h>

using namespace lyra;

static constexpr uint32_t MESH_ASSET_VERSION = 1;

static bool parse_mesh_metadata(const JSON& metadata, MeshAsset* asset)
{
    // 1. version check
    uint32_t version = metadata.value("version", 0u);
    if (version != MESH_ASSET_VERSION) {
        spdlog::error("Failed to load MeshAsset: Unsupported version (found {}, expected {})", version, MESH_ASSET_VERSION);
        return false;
    }

    // 2. extract bounds
    if (metadata.contains("min_bounds")) {
        auto& min         = metadata["min_bounds"];
        asset->min_bounds = Vector3(min[0], min[1], min[2]);
    }
    if (metadata.contains("max_bounds")) {
        auto& max         = metadata["max_bounds"];
        asset->max_bounds = Vector3(max[0], max[1], max[2]);
    }

    // 3. extract index format
    asset->index_format = static_cast<GPUIndexFormat>(metadata.value("index_format", (uint)GPUIndexFormat::UINT32));

    // 4. extract submeshes
    if (metadata.contains("submeshes")) {
        for (auto& sm : metadata["submeshes"]) {
            asset->submeshes.push_back({sm["first_index"].get<uint>(),
                sm["index_count"].get<uint>(),
                sm["first_vertex"].get<uint>(),
                sm["vertex_count"].get<uint>()});
        }
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

    size_t offset   = 0;
    auto   read_raw = [&](void* dest, size_t size) -> bool {
        if (offset + size > content.size()) return false;
        memcpy(dest, content.data() + offset, size);
        offset += size;
        return true;
    };

    // Mesh is now pure binary format:
    // [version: uint32]
    uint32_t version = 0;
    if (!read_raw(&version, sizeof(uint32_t)) || version != MESH_ASSET_VERSION) {
        delete asset;
        return nullptr;
    }

    // [min_bounds: Vector3]
    if (!read_raw(&asset->min_bounds, sizeof(Vector3))) {
        delete asset;
        return nullptr;
    }

    // [max_bounds: Vector3]
    if (!read_raw(&asset->max_bounds, sizeof(Vector3))) {
        delete asset;
        return nullptr;
    }

    // [index_format: uint32]
    uint32_t index_format = 0;
    if (!read_raw(&index_format, sizeof(uint32_t))) {
        delete asset;
        return nullptr;
    }
    asset->index_format = static_cast<GPUIndexFormat>(index_format);

    // [submesh_count: uint32]
    uint32_t submesh_count = 0;
    if (!read_raw(&submesh_count, sizeof(uint32_t))) {
        delete asset;
        return nullptr;
    }

    // [submeshes: array]
    asset->submeshes.resize(submesh_count);
    if (!read_raw(asset->submeshes.data(), submesh_count * sizeof(MeshAsset::Submesh))) {
        delete asset;
        return nullptr;
    }

    // the binary file contains raw data buffers:
    // [vertex_data_size: uint32]
    // [vertex_data: blob]
    // [index_data_size: uint32]
    // [index_data: blob]

    uint32_t vertex_size = 0;
    if (!read_raw(&vertex_size, sizeof(uint32_t))) {
        delete asset;
        return nullptr;
    }
    asset->vertex_data.resize(vertex_size);
    if (!read_raw(asset->vertex_data.data(), vertex_size)) {
        delete asset;
        return nullptr;
    }

    uint32_t index_size = 0;
    if (!read_raw(&index_size, sizeof(uint32_t))) {
        delete asset;
        return nullptr;
    }
    asset->index_data.resize(index_size);
    if (!read_raw(asset->index_data.data(), index_size)) {
        delete asset;
        return nullptr;
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
