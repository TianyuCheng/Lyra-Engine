#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/Format/ModelAsset.h>
#include "ModelUtils.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <fstream>

using namespace lyra;
using namespace lyra::model;

static void configure_obj(AssetServer*, const JSON&) {}

static bool process_obj(JSON& metadata, OSPath source_path, OSPath caches_root)
{
    tinyobj::ObjReaderConfig reader_config;
    reader_config.mtl_search_path = "./"; // Path to look for material files

    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(Path(source_path).string(), reader_config)) {
        if (!reader.Error().empty()) {
            get_logger()->error("tinyobjreader: {}", reader.Error());
        }
        return false;
    }

    if (!reader.Warning().empty()) {
        get_logger()->warn("tinyobjreader: {}", reader.Warning());
    }

    auto& attrib = reader.GetAttrib();
    auto& shapes = reader.GetShapes();
    auto& materials = reader.GetMaterials();

    MeshAsset mesh;
    MeshLOD& lod = mesh.lods.emplace_back();

    // For OBJ, we'll simplify and create one MeshAsset with multiple surfaces (one per shape)
    // and potentially multiple MeshAssets if they are very distinct, but for now let's go with surfaces.

    mesh.min_bounds = Vector3(std::numeric_limits<float>::max());
    mesh.max_bounds = Vector3(std::numeric_limits<float>::lowest());

    Vector<Vector3> positions;
    Vector<Vector3> normals;
    Vector<Vector2> texcoords;
    Vector<uint32_t> indices;

    ModelAsset model;
    model.root = 0;

    // We'll create a single root node for the OBJ
    ModelAsset::Node root_node;
    root_node.name = "OBJ_Root";
    root_node.transform = Matrix4x4(1.0f);

    AssetID mesh_id = random_guid();
    root_node.mesh = AssetHandle<MeshAsset>(mesh_id);

    // Map material index to MaterialAsset GUID
    TreeMap<int, AssetID> material_map;

    for (size_t s = 0; s < shapes.size(); s++) {
        MeshSurface surface;
        surface.slice.first_index = static_cast<uint>(indices.size());
        surface.min_bounds = Vector3(std::numeric_limits<float>::max());
        surface.max_bounds = Vector3(std::numeric_limits<float>::lowest());

        size_t index_offset = 0;
        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
            size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);

            for (size_t v = 0; v < fv; v++) {
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

                Vector3 pos;
                pos.x = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
                pos.y = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
                pos.z = attrib.vertices[3 * size_t(idx.vertex_index) + 2];
                positions.push_back(pos);

                mesh.min_bounds = Vector3(std::min(mesh.min_bounds.x, pos.x), std::min(mesh.min_bounds.y, pos.y), std::min(mesh.min_bounds.z, pos.z));
                mesh.max_bounds = Vector3(std::max(mesh.max_bounds.x, pos.x), std::max(mesh.max_bounds.y, pos.y), std::max(mesh.max_bounds.z, pos.z));
                surface.min_bounds = Vector3(std::min(surface.min_bounds.x, pos.x), std::min(surface.min_bounds.y, pos.y), std::min(surface.min_bounds.z, pos.z));
                surface.max_bounds = Vector3(std::max(surface.max_bounds.x, pos.x), std::max(surface.max_bounds.y, pos.y), std::max(surface.max_bounds.z, pos.z));

                if (idx.normal_index >= 0) {
                    Vector3 norm;
                    norm.x = attrib.normals[3 * size_t(idx.normal_index) + 0];
                    norm.y = attrib.normals[3 * size_t(idx.normal_index) + 1];
                    norm.z = attrib.normals[3 * size_t(idx.normal_index) + 2];
                    normals.push_back(norm);
                }

                if (idx.texcoord_index >= 0) {
                    Vector2 tex;
                    tex.x = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
                    tex.y = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
                    texcoords.push_back(tex);
                }

                indices.push_back(static_cast<uint32_t>(indices.size()));
            }
            index_offset += fv;
        }

        surface.slice.index_count = static_cast<uint>(indices.size()) - surface.slice.first_index;
        surface.slice.first_vertex = surface.slice.first_index;
        surface.slice.vertex_count = surface.slice.index_count;

        // Material handling
        int mat_idx = shapes[s].mesh.material_ids.empty() ? -1 : shapes[s].mesh.material_ids[0];
        if (mat_idx >= 0) {
            if (material_map.find(mat_idx) == material_map.end()) {
                MaterialAsset mat;
                // Simple mapping of OBJ material to Lyra MaterialAsset
                // This would need a proper schema and mapping logic
                AssetID mat_id = random_guid();
                Path mat_path = Path(caches_root) / (std::to_string(mat_id) + ".mat");
                mat.save(mat_path.c_str());
                material_map[mat_idx] = mat_id;
            }
            surface.material = AssetHandle<MaterialAsset>(material_map[mat_idx]);
        }

        lod.surfaces.push_back(surface);
    }

    // Pack attributes
    MeshAttribute& pos_attr = lod.attributes.emplace_back();
    pos_attr.semantics = MeshSemantics::POSITION;
    pos_attr.format = GPUVertexFormat::FLOAT32x3;
    pos_attr.element_count = static_cast<uint>(positions.size());
    pos_attr.data.resize(positions.size() * sizeof(Vector3));
    memcpy(pos_attr.data.data(), positions.data(), pos_attr.data.size());

    if (!normals.empty()) {
        MeshAttribute& norm_attr = lod.attributes.emplace_back();
        norm_attr.semantics = MeshSemantics::NORMAL;
        norm_attr.format = GPUVertexFormat::FLOAT32x3;
        norm_attr.element_count = static_cast<uint>(normals.size());
        norm_attr.data.resize(normals.size() * sizeof(Vector3));
        memcpy(norm_attr.data.data(), normals.data(), norm_attr.data.size());
    }

    if (!texcoords.empty()) {
        MeshAttribute& tex_attr = lod.attributes.emplace_back();
        tex_attr.semantics = MeshSemantics::TEXCOORD0;
        tex_attr.format = GPUVertexFormat::FLOAT32x2;
        tex_attr.element_count = static_cast<uint>(texcoords.size());
        tex_attr.data.resize(texcoords.size() * sizeof(Vector2));
        memcpy(tex_attr.data.data(), texcoords.data(), tex_attr.data.size());
    }

    lod.index_format = GPUIndexFormat::UINT32;
    lod.index_data.resize(indices.size() * sizeof(uint32_t));
    memcpy(lod.index_data.data(), indices.data(), lod.index_data.size());

    Path mesh_cache_path = Path(caches_root) / (std::to_string(mesh_id) + ".mesh");
    mesh.save(mesh_cache_path.c_str());

    model.nodes.push_back(root_node);

    // Save ModelAsset
    JSON model_json;
    model_json["root"] = 0;
    JSON nodes_arr = JSON::array();
    for (const auto& node : model.nodes) {
        JSON n;
        n["name"] = node.name;
        if (node.mesh.valid())     n["mesh"]     = std::to_string(node.mesh.uuid);
        if (node.material.valid()) n["material"] = std::to_string(node.material.uuid);

        JSON transform = JSON::array();
        for (int r = 0; r < 4; ++r) {
            JSON row = JSON::array();
            for (int c = 0; c < 4; ++c) row.push_back(node.transform[r][c]);
            transform.push_back(row);
        }
        n["transform"] = transform;

        nodes_arr.push_back(n);
    }
    model_json["nodes"] = nodes_arr;

    Path model_cache_path = Path(caches_root) / (std::to_string(metadata["guid"].get<AssetID>()) + ".model");
    std::ofstream out(model_cache_path);
    out << model_json.dump(4);

    metadata["path"] = std::to_string(metadata["guid"].get<AssetID>()) + ".model";

    JSON deps = JSON::array();
    deps.push_back(mesh_id);
    for (auto const& [idx, id] : material_map) deps.push_back(id);
    metadata["dependencies"] = deps;

    return true;
}

static uint get_obj_extensions(CString* extensions)
{
    if (extensions) {
        extensions[0] = ".obj";
    }
    return 1;
}

namespace lyra::obj::cooker
{
    void prepare() {}
    void cleanup() {}
    auto create() -> AssetCookerAPI
    {
        auto api = AssetCookerAPI{};
        api.configure = configure_obj;
        api.process = process_obj;
        api.get_supported_extensions = get_obj_extensions;
        return api;
    }
}
