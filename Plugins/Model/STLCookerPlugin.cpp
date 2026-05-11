#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/Format/ModelAsset.h>

#include <fstream>
#include <numeric>

using namespace lyra;

static void configure_stl(AssetServer*, const JSON&) {}

static bool process_stl(JSON& metadata, OSPath source_path, OSPath caches_root)
{
    std::ifstream file(source_path, std::ios::binary);
    if (!file.is_open()) return false;

    // determine if binary or ascii
    char header[80];
    file.read(header, 80);
    bool is_binary = true;
    if (String(header, 5) == "solid") {
        // could still be binary if the file is large, but usually "solid" means ascii
        // simplified check:
        is_binary = false;
    }

    MeshAsset mesh;
    MeshLOD& lod = mesh.lods.emplace_back();
    MeshAttribute& pos_attr = lod.attributes.emplace_back();
    pos_attr.semantics = MeshSemantics::POSITION;
    pos_attr.format = GPUVertexFormat::FLOAT32x3;

    MeshAttribute& norm_attr = lod.attributes.emplace_back();
    norm_attr.semantics = MeshSemantics::NORMAL;
    norm_attr.format = GPUVertexFormat::FLOAT32x3;

    Vector<Vector3> positions;
    Vector<Vector3> normals;

    if (is_binary) {
        uint32_t triangle_count;
        file.read(reinterpret_cast<char*>(&triangle_count), 4);
        
        positions.reserve(triangle_count * 3);
        normals.reserve(triangle_count * 3);

        for (uint32_t i = 0; i < triangle_count; ++i) {
            Vector3 n, v0, v1, v2;
            uint16_t attr_byte_count;

            file.read(reinterpret_cast<char*>(&n), 12);
            file.read(reinterpret_cast<char*>(&v0), 12);
            file.read(reinterpret_cast<char*>(&v1), 12);
            file.read(reinterpret_cast<char*>(&v2), 12);
            file.read(reinterpret_cast<char*>(&attr_byte_count), 2);

            normals.push_back(n); normals.push_back(n); normals.push_back(n);
            positions.push_back(v0); positions.push_back(v1); positions.push_back(v2);
        }
    } else {
        // simplistic ASCII STL parser
        file.seekg(0);
        String line;
        Vector3 n;
        while (std::getline(file, line)) {
            if (line.find("facet normal") != String::npos) {
                sscanf(line.c_str(), " facet normal %f %f %f", &n.x, &n.y, &n.z);
            } else if (line.find("vertex") != String::npos) {
                Vector3 v;
                sscanf(line.c_str(), " vertex %f %f %f", &v.x, &v.y, &v.z);
                positions.push_back(v);
                normals.push_back(n);
            }
        }
    }

    if (positions.empty()) return false;

    pos_attr.element_count = static_cast<uint>(positions.size());
    pos_attr.data.resize(positions.size() * sizeof(Vector3));
    memcpy(pos_attr.data.data(), positions.data(), pos_attr.data.size());

    norm_attr.element_count = static_cast<uint>(normals.size());
    norm_attr.data.resize(normals.size() * sizeof(Vector3));
    memcpy(norm_attr.data.data(), normals.data(), norm_attr.data.size());

    lod.index_format = GPUIndexFormat::UINT32;
    lod.index_data.resize(positions.size() * sizeof(uint32_t));
    uint32_t* indices = reinterpret_cast<uint32_t*>(lod.index_data.data());
    std::iota(indices, indices + positions.size(), 0);

    MeshSurface& surf = lod.surfaces.emplace_back();
    surf.slice.first_index = 0;
    surf.slice.index_count = static_cast<uint>(positions.size());
    surf.slice.first_vertex = 0;
    surf.slice.vertex_count = static_cast<uint>(positions.size());

    // Calculate bounds
    mesh.min_bounds = Vector3(std::numeric_limits<float>::max());
    mesh.max_bounds = Vector3(std::numeric_limits<float>::lowest());
    for (const auto& v : positions) {
        mesh.min_bounds = Vector3(std::min(mesh.min_bounds.x, v.x), std::min(mesh.min_bounds.y, v.y), std::min(mesh.min_bounds.z, v.z));
        mesh.max_bounds = Vector3(std::max(mesh.max_bounds.x, v.x), std::max(mesh.max_bounds.y, v.y), std::max(mesh.max_bounds.z, v.z));
    }
    surf.min_bounds = mesh.min_bounds;
    surf.max_bounds = mesh.max_bounds;

    AssetID mesh_id = random_guid();
    Path mesh_cache_path = Path(caches_root) / (std::to_string(mesh_id) + ".mesh");
    mesh.save(mesh_cache_path.c_str());

    // Create ModelAsset metadata
    JSON model_json;
    model_json["root"] = 0;
    JSON node;
    node["name"] = "STL_Model";
    node["mesh"] = std::to_string(mesh_id);
    model_json["nodes"] = JSON::array({node});

    Path model_cache_path = Path(caches_root) / (std::to_string(metadata["guid"].get<AssetID>()) + ".model");
    std::ofstream out(model_cache_path);
    out << model_json.dump(4);

    metadata["path"] = std::to_string(metadata["guid"].get<AssetID>()) + ".model";
    metadata["dependencies"] = JSON::array({mesh_id});

    return true;
}

static uint get_stl_extensions(CString* extensions)
{
    if (extensions) {
        extensions[0] = ".stl";
    }
    return 1;
}

namespace lyra::stl::cooker
{
    void prepare() {}
    void cleanup() {}
    auto create() -> AssetCookerAPI
    {
        auto api = AssetCookerAPI{};
        api.configure = configure_stl;
        api.process = process_stl;
        api.get_supported_extensions = get_stl_extensions;
        return api;
    }
}
