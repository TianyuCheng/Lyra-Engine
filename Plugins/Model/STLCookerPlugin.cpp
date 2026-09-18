#include <numeric>
#include <fstream>
#include <filesystem>

#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/Assets/AMSPreview.h>
#include <Lyra/Assets/Format/ModelAsset.h>
#include "ModelUtils.h"

using namespace lyra;
using namespace lyra::model;

namespace fs = std::filesystem;

static void configure_stl(AssetServer*, const JSON&) {}

static bool process_stl(JSON& metadata, OSPath source_path, OSPath caches_root)
{
    try {
        std::ifstream file(source_path, std::ios::binary);
        if (!file.is_open()) {
            get_logger()->error("failed to open STL file: {}", Path(source_path).string());
            return false;
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

        // determine if binary or ascii
        std::error_code ec;
        uintmax_t       file_size      = fs::file_size(source_path, ec);
        bool            is_binary      = false;
        uint            triangle_count = 0;

        if (!ec && file_size >= 84) {
            file.seekg(80);
            file.read(reinterpret_cast<char*>(&triangle_count), 4);
            if (file_size == 84 + static_cast<ulong>(triangle_count) * 50) {
                is_binary = true;
            } else {
                file.seekg(0);
                char header[80] = {};
                file.read(header, 80);
                is_binary = (StringView(header, 5) != "solid");
            }
        }

        Vector<Vector3> positions;
        Vector<Vector3> normals;

        if (is_binary) {
            constexpr uint max_triangles = 50'000'000;
            if (triangle_count > max_triangles) {
                get_logger()->error("STL triangle count too large ({}): {}", triangle_count, Path(source_path).string());
                return false;
            }

            positions.reserve(triangle_count * 3);
            normals.reserve(triangle_count * 3);

            for (uint i = 0; i < triangle_count; ++i) {
                Vector3 n, v0, v1, v2;
                ushort  attr_byte_count;

                if (!file.read(reinterpret_cast<char*>(&n), 12) ||
                    !file.read(reinterpret_cast<char*>(&v0), 12) ||
                    !file.read(reinterpret_cast<char*>(&v1), 12) ||
                    !file.read(reinterpret_cast<char*>(&v2), 12) ||
                    !file.read(reinterpret_cast<char*>(&attr_byte_count), 2)) {
                    get_logger()->warn("Premature EOF in binary STL after {} triangles: {}", i, Path(source_path).string());
                    break;
                }

                normals.push_back(n);
                normals.push_back(n);
                normals.push_back(n);
                positions.push_back(v0);
                positions.push_back(v1);
                positions.push_back(v2);
            }
        } else {
            file.seekg(0);
            String  line;
            Vector3 n;
            while (std::getline(file, line)) {
                if (line.find("facet normal") != String::npos) {
                    sscanf(line.c_str(), " facet normal %f %f %f", &n.x, &n.y, &n.z);
                } else if (line.find("vertex") != String::npos) {
                    Vector3 v;
                    if (sscanf(line.c_str(), " vertex %f %f %f", &v.x, &v.y, &v.z) == 3) {
                        positions.push_back(v);
                        normals.push_back(n);
                    }
                }
            }
        }

        if (positions.empty()) {
            get_logger()->error("stl file contains no geometry: {}", Path(source_path).string());
            return false;
        }

        // STL is a 3D printing / CAD format with Z-up convention.
        // Convert Z-up to engine-native Y-up (-90 deg rotation around X).
        bool convert_z_up = true;
        if (metadata.contains("z_up") && metadata["z_up"].is_boolean()) {
            convert_z_up = metadata["z_up"].get<bool>();
        }

        if (convert_z_up) {
            float yaw_adjust = 90.0f;
            if (metadata.contains("rotation_y") && metadata["rotation_y"].is_number()) {
                yaw_adjust = metadata["rotation_y"].get<float>();
            }
            Matrix4x4 rot = glm::rotate(Matrix4x4(1.0f), glm::radians(yaw_adjust), Vector3(0.0f, 1.0f, 0.0f)) *
                            glm::rotate(Matrix4x4(1.0f), glm::radians(-90.0f), Vector3(1.0f, 0.0f, 0.0f));
            Matrix3x3 norm_rot = Matrix3x3(rot);
            for (auto& v : positions) {
                v = Vector3(rot * Vector4(v, 1.0f));
            }
            for (auto& n : normals) {
                n = glm::normalize(norm_rot * n);
            }
        }

        MeshAsset mesh;
        MeshLOD&  lod = mesh.lods.emplace_back();

        MeshAttribute& pos_attr = lod.attributes.emplace_back();
        pos_attr.semantics      = MeshSemantics::POSITION;
        pos_attr.format         = GPUVertexFormat::FLOAT32x3;
        pos_attr.element_count  = static_cast<uint>(positions.size());
        pos_attr.data.resize(positions.size() * sizeof(Vector3));
        memcpy(pos_attr.data.data(), positions.data(), pos_attr.data.size());

        MeshAttribute& norm_attr = lod.attributes.emplace_back();
        norm_attr.semantics      = MeshSemantics::NORMAL;
        norm_attr.format         = GPUVertexFormat::FLOAT32x3;
        norm_attr.element_count  = static_cast<uint>(normals.size());
        norm_attr.data.resize(normals.size() * sizeof(Vector3));
        memcpy(norm_attr.data.data(), normals.data(), norm_attr.data.size());

        lod.index_format = GPUIndexFormat::UINT32;
        lod.index_data.resize(positions.size() * sizeof(uint));
        uint* indices = reinterpret_cast<uint*>(lod.index_data.data());
        std::iota(indices, indices + positions.size(), 0);

        MeshSurface& surf       = lod.surfaces.emplace_back();
        surf.slice.first_index  = 0;
        surf.slice.index_count  = static_cast<uint>(positions.size());
        surf.slice.first_vertex = 0;
        surf.slice.vertex_count = static_cast<uint>(positions.size());

        mesh.min_bounds = Vector3(std::numeric_limits<float>::max());
        mesh.max_bounds = Vector3(std::numeric_limits<float>::lowest());
        for (const auto& v : positions) {
            mesh.min_bounds = Vector3(std::min(mesh.min_bounds.x, v.x), std::min(mesh.min_bounds.y, v.y), std::min(mesh.min_bounds.z, v.z));
            mesh.max_bounds = Vector3(std::max(mesh.max_bounds.x, v.x), std::max(mesh.max_bounds.y, v.y), std::max(mesh.max_bounds.z, v.z));
        }
        surf.min_bounds = mesh.min_bounds;
        surf.max_bounds = mesh.max_bounds;

        if (!metadata.contains("guid") || !metadata["guid"].is_number()) {
            return false;
        }

        AssetID model_id = metadata["guid"].get<AssetID>();

        AssetDependencyScope deps(metadata, source_path, caches_root);
        AssetID              mesh_id = deps.resolve(model_id, "default", MeshAsset::type);
        deps.set_path(mesh_id, "meshes/" + std::to_string(mesh_id) + ".mesh");

        Path mesh_cache_path = meshes_dir / (std::to_string(mesh_id) + ".mesh");
        MeshAsset::saver().save(&mesh, mesh_cache_path.c_str());

        ModelAsset model;
        model.root = 0;

        ModelAsset::Node root_node;
        root_node.name      = "STL_Model";
        root_node.mesh      = AssetHandle<MeshAsset>(mesh_id);
        root_node.transform = Matrix4x4(1.0f);
        model.nodes.push_back(root_node);

        Path model_cache_path = models_dir / (std::to_string(model_id) + ".model");
        ModelAsset::saver().save(&model, model_cache_path.c_str());

        metadata["path"] = "models/" + std::to_string(model_id) + ".model";
        deps.commit();

        Vector<uint> thumb_indices(positions.size());
        std::iota(thumb_indices.begin(), thumb_indices.end(), 0);
        preview_api().generate_thumbnail(PreviewScene::make_mesh(positions, normals, {}, thumb_indices), metadata);

        return true;
    } catch (const std::exception& e) {
        get_logger()->error("Exception while cooking STL file {}: {}", Path(source_path).string(), e.what());
        return false;
    } catch (...) {
        get_logger()->error("Unknown error while cooking STL file {}", Path(source_path).string());
        return false;
    }
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
        auto api                     = AssetCookerAPI{};
        api.configure                = configure_stl;
        api.process                  = process_stl;
        api.get_supported_extensions = get_stl_extensions;
        return api;
    }
} // namespace lyra::stl::cooker
