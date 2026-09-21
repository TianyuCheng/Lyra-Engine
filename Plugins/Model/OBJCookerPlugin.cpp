#include <filesystem>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/Assets/AMSPreview.h>
#include <Lyra/Assets/Format/ModelAsset.h>
#include "ModelUtils.h"

using namespace lyra;
using namespace lyra::model;

namespace fs = std::filesystem;

static void configure_obj(AssetServer*, const JSON&) {}

static bool process_obj(JSON& metadata, OSPath source_path, OSPath caches_root)
{
    try {
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

        // cache subdirectories
        fs::path root(caches_root);
        fs::path models_dir    = root / "models";
        fs::path meshes_dir    = root / "meshes";
        fs::path textures_dir  = root / "textures";
        fs::path materials_dir = root / "materials";
        fs::create_directories(models_dir);
        fs::create_directories(meshes_dir);
        fs::create_directories(textures_dir);
        fs::create_directories(materials_dir);

        auto& attrib    = reader.GetAttrib();
        auto& shapes    = reader.GetShapes();
        auto& materials = reader.GetMaterials();

        MeshAsset mesh;
        MeshLOD&  lod = mesh.lods.emplace_back();

        mesh.min_bounds = Vector3(std::numeric_limits<float>::max());
        mesh.max_bounds = Vector3(std::numeric_limits<float>::lowest());

        Vector<Vector3> positions;
        Vector<Vector3> normals;
        Vector<Vector2> texcoords;
        Vector<uint>    indices;

        if (!metadata.contains("guid") || !metadata["guid"].is_number()) {
            return false;
        }

        AssetID              model_id = metadata["guid"].get<AssetID>();
        AssetDependencyScope deps(metadata, source_path, caches_root);

        ModelAsset model;
        model.root = 0;

        ModelAsset::Node root_node;
        root_node.name      = "OBJ_Root";
        root_node.transform = Matrix4x4(1.0f);

        AssetID mesh_id = deps.resolve(model_id, "mesh/0", MeshAsset::type);
        deps.set_path(mesh_id, "meshes/" + std::to_string(mesh_id) + ".mesh");
        root_node.mesh = AssetHandle<MeshAsset>(mesh_id);

        TreeMap<int, AssetID> material_map;

        for (size_t s = 0; s < shapes.size(); s++) {
            MeshSurface surface;
            surface.slice.first_index = static_cast<uint>(indices.size());
            surface.min_bounds        = Vector3(std::numeric_limits<float>::max());
            surface.max_bounds        = Vector3(std::numeric_limits<float>::lowest());

            size_t index_offset = 0;
            for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
                size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);

                for (size_t v = 0; v < fv; v++) {
                    if (index_offset + v >= shapes[s].mesh.indices.size()) break;
                    tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

                    if (idx.vertex_index < 0 || size_t(idx.vertex_index) * 3 + 2 >= attrib.vertices.size()) {
                        continue;
                    }

                    Vector3 pos;
                    pos.x = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
                    pos.y = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
                    pos.z = attrib.vertices[3 * size_t(idx.vertex_index) + 2];
                    positions.push_back(pos);

                    mesh.min_bounds    = Vector3(std::min(mesh.min_bounds.x, pos.x), std::min(mesh.min_bounds.y, pos.y), std::min(mesh.min_bounds.z, pos.z));
                    mesh.max_bounds    = Vector3(std::max(mesh.max_bounds.x, pos.x), std::max(mesh.max_bounds.y, pos.y), std::max(mesh.max_bounds.z, pos.z));
                    surface.min_bounds = Vector3(std::min(surface.min_bounds.x, pos.x), std::min(surface.min_bounds.y, pos.y), std::min(surface.min_bounds.z, pos.z));
                    surface.max_bounds = Vector3(std::max(surface.max_bounds.x, pos.x), std::max(surface.max_bounds.y, pos.y), std::max(surface.max_bounds.z, pos.z));

                    if (idx.normal_index >= 0 && size_t(idx.normal_index) * 3 + 2 < attrib.normals.size()) {
                        Vector3 norm;
                        norm.x = attrib.normals[3 * size_t(idx.normal_index) + 0];
                        norm.y = attrib.normals[3 * size_t(idx.normal_index) + 1];
                        norm.z = attrib.normals[3 * size_t(idx.normal_index) + 2];
                        normals.push_back(norm);
                    }

                    if (idx.texcoord_index >= 0 && size_t(idx.texcoord_index) * 2 + 1 < attrib.texcoords.size()) {
                        Vector2 tex;
                        tex.x = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
                        tex.y = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
                        texcoords.push_back(tex);
                    }

                    indices.push_back(static_cast<uint>(indices.size()));
                }
                index_offset += fv;
            }

            surface.slice.index_count  = static_cast<uint>(indices.size()) - surface.slice.first_index;
            surface.slice.first_vertex = surface.slice.first_index;
            surface.slice.vertex_count = surface.slice.index_count;

            int mat_idx = shapes[s].mesh.material_ids.empty() ? -1 : shapes[s].mesh.material_ids[0];
            if (mat_idx >= 0) {
                if (material_map.find(mat_idx) == material_map.end()) {
                    MaterialAsset mat;
                    String        mat_name = "material/" + std::to_string(mat_idx);
                    if (static_cast<size_t>(mat_idx) < materials.size()) {
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
                }
                surface.material = AssetHandle<MaterialAsset>(material_map[mat_idx]);
            }

            lod.surfaces.push_back(surface);
        }

        if (positions.empty()) {
            get_logger()->error("OBJ file contains no valid geometry: {}", Path(source_path).string());
            return false;
        }

        MeshAttribute& pos_attr = lod.attributes.emplace_back();
        pos_attr.semantics      = MeshSemantics::POSITION;
        pos_attr.format         = GPUVertexFormat::FLOAT32x3;
        pos_attr.element_count  = static_cast<uint>(positions.size());
        pos_attr.data.resize(positions.size() * sizeof(Vector3));
        memcpy(pos_attr.data.data(), positions.data(), pos_attr.data.size());

        if (!normals.empty()) {
            MeshAttribute& norm_attr = lod.attributes.emplace_back();
            norm_attr.semantics      = MeshSemantics::NORMAL;
            norm_attr.format         = GPUVertexFormat::FLOAT32x3;
            norm_attr.element_count  = static_cast<uint>(normals.size());
            norm_attr.data.resize(normals.size() * sizeof(Vector3));
            memcpy(norm_attr.data.data(), normals.data(), norm_attr.data.size());
        }

        if (!texcoords.empty()) {
            MeshAttribute& tex_attr = lod.attributes.emplace_back();
            tex_attr.semantics      = MeshSemantics::TEXCOORD0;
            tex_attr.format         = GPUVertexFormat::FLOAT32x2;
            tex_attr.element_count  = static_cast<uint>(texcoords.size());
            tex_attr.data.resize(texcoords.size() * sizeof(Vector2));
            memcpy(tex_attr.data.data(), texcoords.data(), tex_attr.data.size());
        }

        lod.index_format = GPUIndexFormat::UINT32;
        lod.index_data.resize(indices.size() * sizeof(uint));
        memcpy(lod.index_data.data(), indices.data(), lod.index_data.size());

        Path mesh_cache_path = meshes_dir / (std::to_string(mesh_id) + ".mesh");
        MeshAsset::saver().save(&mesh, mesh_cache_path.c_str());

        model.nodes.push_back(root_node);

        Path model_cache_path = models_dir / (std::to_string(model_id) + ".model");
        ModelAsset::saver().save(&model, model_cache_path.c_str());

        metadata["path"] = "models/" + std::to_string(model_id) + ".model";
        deps.commit();

        preview_api().generate_thumbnail(PreviewScene::make_mesh(positions, normals, texcoords, indices), metadata);

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
