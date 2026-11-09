#include <filesystem>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <Lyra/Utilities/Logger.h>
#include <Lyra/Utilities/Plugin.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/Assets/AMSPreview.h>
#include <Lyra/Assets/Format/ModelAsset.h>
#include "ModelUtils.h"

using namespace lyra;
using namespace lyra::model;

static void configure_obj(AssetServer*, const JSON&) {}

struct OBJRawGeometry
{
    Vector<Vector3> positions;
    Vector<Vector3> normals;
    Vector<Vector2> texcoords;
    Vector<uint>    indices;
    Vector3         min_bounds{std::numeric_limits<float>::max()};
    Vector3         max_bounds{std::numeric_limits<float>::lowest()};
};

static void update_bounds(Vector3& min_b, Vector3& max_b, const Vector3& pos)
{
    min_b = Vector3(std::min(min_b.x, pos.x), std::min(min_b.y, pos.y), std::min(min_b.z, pos.z));
    max_b = Vector3(std::max(max_b.x, pos.x), std::max(max_b.y, pos.y), std::max(max_b.z, pos.z));
}

static void extract_face_vertex(
    const tinyobj::attrib_t& attrib,
    const tinyobj::index_t&  idx,
    OBJRawGeometry&          geom,
    MeshSurface&             surface)
{
    if (idx.vertex_index < 0 || usize(idx.vertex_index) * 3 + 2 >= attrib.vertices.size()) {
        return;
    }

    Vector3 pos;
    pos.x = attrib.vertices[3 * usize(idx.vertex_index) + 0];
    pos.y = attrib.vertices[3 * usize(idx.vertex_index) + 1];
    pos.z = attrib.vertices[3 * usize(idx.vertex_index) + 2];
    geom.positions.push_back(pos);

    update_bounds(geom.min_bounds, geom.max_bounds, pos);
    update_bounds(surface.min_bounds, surface.max_bounds, pos);

    if (idx.normal_index >= 0 && usize(idx.normal_index) * 3 + 2 < attrib.normals.size()) {
        Vector3 norm;
        norm.x = attrib.normals[3 * usize(idx.normal_index) + 0];
        norm.y = attrib.normals[3 * usize(idx.normal_index) + 1];
        norm.z = attrib.normals[3 * usize(idx.normal_index) + 2];
        geom.normals.push_back(norm);
    }

    if (idx.texcoord_index >= 0 && usize(idx.texcoord_index) * 2 + 1 < attrib.texcoords.size()) {
        Vector2 tex;
        tex.x = attrib.texcoords[2 * usize(idx.texcoord_index) + 0];
        tex.y = attrib.texcoords[2 * usize(idx.texcoord_index) + 1];
        geom.texcoords.push_back(tex);
    }

    geom.indices.push_back(static_cast<uint>(geom.indices.size()));
}

static auto get_or_cook_material(
    int                                mat_idx,
    const Vector<tinyobj::material_t>& materials,
    AssetID                            model_id,
    AssetDependencyScope&              deps,
    const Path&                        materials_dir,
    TreeMap<int, AssetID>&             material_map) -> AssetID
{
    if (mat_idx < 0) {
        return 0;
    }

    auto it = material_map.find(mat_idx);
    if (it != material_map.end()) {
        return it->second;
    }

    MaterialAsset mat;
    String        mat_name = "material/" + std::to_string(mat_idx);
    if (static_cast<usize>(mat_idx) < materials.size()) {
        const auto& m = materials[mat_idx];
        if (!m.name.empty()) mat_name = "material/" + m.name;
        mat.base_color_factor = Vector4(m.diffuse[0], m.diffuse[1], m.diffuse[2], 1.0f);
        mat.roughness_factor  = 1.0f - (m.shininess / 1000.0f);
        if (mat.roughness_factor < 0.05f) mat.roughness_factor = 0.05f;
    }

    AssetID mat_id = deps.resolve(model_id, mat_name, MaterialAsset::type);
    deps.set_path(mat_id, "materials/" + std::to_string(mat_id) + ".material");
    Path mat_path = materials_dir / (std::to_string(mat_id) + ".material");
    MaterialAsset::saver().save(&mat, mat_path.c_str());

    material_map[mat_idx] = mat_id;
    return mat_id;
}

static auto extract_shape_surface(
    const tinyobj::shape_t&            shape,
    const tinyobj::attrib_t&           attrib,
    const Vector<tinyobj::material_t>& materials,
    AssetID                            model_id,
    AssetDependencyScope&              deps,
    const Path&                        materials_dir,
    TreeMap<int, AssetID>&             material_map,
    OBJRawGeometry&                    geom) -> MeshSurface
{
    MeshSurface surface;
    surface.slice.first_index = static_cast<uint>(geom.indices.size());
    surface.min_bounds        = Vector3(std::numeric_limits<float>::max());
    surface.max_bounds        = Vector3(std::numeric_limits<float>::lowest());

    usize index_offset = 0;
    for (usize f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
        usize fv = usize(shape.mesh.num_face_vertices[f]);
        for (usize v = 0; v < fv; v++) {
            if (index_offset + v >= shape.mesh.indices.size()) break;
            extract_face_vertex(attrib, shape.mesh.indices[index_offset + v], geom, surface);
        }
        index_offset += fv;
    }

    surface.slice.index_count  = static_cast<uint>(geom.indices.size()) - surface.slice.first_index;
    surface.slice.first_vertex = surface.slice.first_index;
    surface.slice.vertex_count = surface.slice.index_count;

    int     mat_idx = shape.mesh.material_ids.empty() ? -1 : shape.mesh.material_ids[0];
    AssetID mat_id  = get_or_cook_material(mat_idx, materials, model_id, deps, materials_dir, material_map);
    if (mat_id != 0) {
        surface.material = AssetHandle<MaterialAsset>(mat_id);
    }

    return surface;
}

static auto build_mesh_asset(const OBJRawGeometry& geom, Vector<MeshSurface> surfaces) -> MeshAsset
{
    MeshAsset mesh;
    mesh.min_bounds = geom.min_bounds;
    mesh.max_bounds = geom.max_bounds;

    MeshLOD& lod = mesh.lods.emplace_back();
    lod.surfaces = std::move(surfaces);

    MeshAttribute& pos_attr = lod.attributes.emplace_back();
    pos_attr.semantics      = MeshSemantics::POSITION;
    pos_attr.format         = GPUVertexFormat::FLOAT32x3;
    pos_attr.element_count  = static_cast<uint>(geom.positions.size());
    pos_attr.data.resize(geom.positions.size() * sizeof(Vector3));
    memcpy(pos_attr.data.data(), geom.positions.data(), pos_attr.data.size());

    if (!geom.normals.empty()) {
        MeshAttribute& norm_attr = lod.attributes.emplace_back();
        norm_attr.semantics      = MeshSemantics::NORMAL;
        norm_attr.format         = GPUVertexFormat::FLOAT32x3;
        norm_attr.element_count  = static_cast<uint>(geom.normals.size());
        norm_attr.data.resize(geom.normals.size() * sizeof(Vector3));
        memcpy(norm_attr.data.data(), geom.normals.data(), norm_attr.data.size());
    }

    if (!geom.texcoords.empty()) {
        MeshAttribute& tex_attr = lod.attributes.emplace_back();
        tex_attr.semantics      = MeshSemantics::TEXCOORD0;
        tex_attr.format         = GPUVertexFormat::FLOAT32x2;
        tex_attr.element_count  = static_cast<uint>(geom.texcoords.size());
        tex_attr.data.resize(geom.texcoords.size() * sizeof(Vector2));
        memcpy(tex_attr.data.data(), geom.texcoords.data(), tex_attr.data.size());
    }

    lod.index_format = GPUIndexFormat::UINT32;
    lod.index_data.resize(geom.indices.size() * sizeof(uint));
    memcpy(lod.index_data.data(), geom.indices.data(), lod.index_data.size());

    return mesh;
}

static bool process_obj(JSON& metadata, OSPath source_path, OSPath caches_root)
{
    try {
        if (!metadata.contains("guid") || !metadata["guid"].is_number()) {
            return false;
        }

        tinyobj::ObjReaderConfig reader_config;
        reader_config.mtl_search_path = "./";

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

        CacheDirs            cache    = ensure_cache_dirs(caches_root);
        AssetID              model_id = metadata["guid"].get<AssetID>();
        AssetDependencyScope deps(metadata, source_path, caches_root);

        auto& attrib    = reader.GetAttrib();
        auto& shapes    = reader.GetShapes();
        auto& materials = reader.GetMaterials();

        OBJRawGeometry        geom;
        Vector<MeshSurface>   surfaces;
        TreeMap<int, AssetID> material_map;

        for (const auto& shape : shapes) {
            surfaces.push_back(extract_shape_surface(
                shape, attrib, materials, model_id, deps, cache.materials, material_map, geom));
        }

        if (geom.positions.empty()) {
            get_logger()->error("OBJ file contains no valid geometry: {}", Path(source_path).string());
            return false;
        }

        AssetID mesh_id = deps.resolve(model_id, "mesh/0", MeshAsset::type);
        deps.set_path(mesh_id, "meshes/" + std::to_string(mesh_id) + ".mesh");

        MeshAsset mesh            = build_mesh_asset(geom, std::move(surfaces));
        Path      mesh_cache_path = cache.meshes / (std::to_string(mesh_id) + ".mesh");
        MeshAsset::saver().save(&mesh, mesh_cache_path.c_str());

        ModelAsset model;
        model.root = 0;

        ModelAsset::Node root_node;
        String           stem_name = Path(source_path).stem().string();
        root_node.name             = !stem_name.empty() ? stem_name : "OBJ_Root";
        root_node.transform        = Matrix4x4(1.0f);
        root_node.mesh             = AssetHandle<MeshAsset>(mesh_id);
        model.nodes.push_back(root_node);

        Path model_cache_path = cache.models / (std::to_string(model_id) + ".model");
        ModelAsset::saver().save(&model, model_cache_path.c_str());

        metadata["path"] = "models/" + std::to_string(model_id) + ".model";
        deps.commit();

        preview_api().generate_thumbnail(
            PreviewScene::make_mesh(geom.positions, geom.normals, geom.texcoords, geom.indices), metadata);

        return true;
    } catch (const std::exception& e) {
        get_logger()->error("Exception while cooking OBJ file {}: {}", Path(source_path).string(), e.what());
        return false;
    } catch (...) {
        get_logger()->error("Unknown error while cooking OBJ file {}", Path(source_path).string());
        return false;
    }
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
        auto api                     = AssetCookerAPI{};
        api.configure                = configure_obj;
        api.process                  = process_obj;
        api.get_supported_extensions = get_obj_extensions;
        return api;
    }
} // namespace lyra::obj::cooker
