#include <algorithm>
#include <limits>
#include <Lyra/Utilities/Plugin.h>
#include <Lyra/Assets/AMSPreview.h>

using namespace lyra;

FORWARD_DECLARE_API(lyra::preview, AssetPreviewAPI)

using PreviewPlugin = BuiltinPlugin<AssetPreviewAPI>;

auto PreviewScene::calculate_bounds() const -> std::pair<Vector3, Vector3>
{
    Vector3 aabb_min(std::numeric_limits<float>::max());
    Vector3 aabb_max(std::numeric_limits<float>::lowest());

    for (const auto& mesh : meshes) {
        for (const auto& p : mesh.positions) {
            Vector4 world_p = mesh.transform * Vector4(p, 1.0f);
            Vector3 wp      = Vector3(world_p) / (world_p.w != 0.0f ? world_p.w : 1.0f);
            aabb_min        = glm::min(aabb_min, wp);
            aabb_max        = glm::max(aabb_max, wp);
        }
    }

    return {aabb_min, aabb_max};
}

auto PreviewScene::make_textured_quad(PreviewTexture texture) -> PreviewScene
{
    PreviewScene scene;
    scene.is_flat = true;
    scene.textures.push_back(std::move(texture));

    PreviewMaterial mat;
    mat.albedo_texture_id = 0;
    scene.materials.push_back(mat);

    PreviewMesh mesh;
    mesh.material_id = 0;
    mesh.positions   = {
        Vector3(-1.0f, -1.0f, 0.0f),
        Vector3( 1.0f, -1.0f, 0.0f),
        Vector3( 1.0f,  1.0f, 0.0f),
        Vector3(-1.0f,  1.0f, 0.0f)
    };
    mesh.normals = {
        Vector3(0.0f, 0.0f, 1.0f),
        Vector3(0.0f, 0.0f, 1.0f),
        Vector3(0.0f, 0.0f, 1.0f),
        Vector3(0.0f, 0.0f, 1.0f)
    };
    mesh.uvs = {
        Vector2(0.0f, 1.0f),
        Vector2(1.0f, 1.0f),
        Vector2(1.0f, 0.0f),
        Vector2(0.0f, 0.0f)
    };
    mesh.indices = {0, 1, 2, 0, 2, 3};

    scene.meshes.push_back(std::move(mesh));
    return scene;
}

auto PreviewScene::make_mesh(
    Vector<Vector3> positions,
    Vector<Vector3> normals,
    Vector<Vector2> uvs,
    Vector<uint>    indices) -> PreviewScene
{
    PreviewScene scene;
    PreviewMesh  mesh;
    mesh.positions = std::move(positions);
    mesh.normals   = std::move(normals);
    mesh.uvs       = std::move(uvs);
    mesh.indices   = std::move(indices);
    mesh.transform = Matrix4x4(1.0f);
    scene.meshes.push_back(std::move(mesh));
    return scene;
}

auto lyra::preview_api() -> AssetPreviewAPI&
{
    static Own<PreviewPlugin> PLUGIN;
    if (!PLUGIN) {
        PLUGIN = std::make_unique<PreviewPlugin>(
            lyra::preview::create,
            lyra::preview::prepare,
            lyra::preview::cleanup
        );
    }
    return *PLUGIN->get_api();
}
