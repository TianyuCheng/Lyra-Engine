#include "helper.h"
#include <Lyra/Format/MaterialAsset.h>
#include <Lyra/Format/ModelAsset.h>
#include <Lyra/Format/SceneAsset.h>
#include <Lyra/Assets/AMSServer.h>
#include <Lyra/Format/MeshAsset.h>
#include <Lyra/Format/TextAsset.h>
#include <Lyra/Format/TomlAsset.h>
#include <Lyra/Format/JsonAsset.h>

namespace fs = std::filesystem;

using namespace lyra;

TEST_CASE("mat::material_usd_serialization")
{
    auto temp_dir = fs::temp_directory_path() / "lyra_mat_usd_test";
    fs::create_directories(temp_dir);

    auto mat_path = temp_dir / "test.material";

    MaterialAsset original;
    original.base_color_factor  = Vector4(0.8f, 0.2f, 0.1f, 0.9f);
    original.metallic_factor    = 0.75f;
    original.roughness_factor   = 0.35f;
    original.emissive_factor    = Vector3(0.1f, 0.2f, 0.3f);
    original.occlusion_strength = 0.8f;
    original.normal_scale       = 1.2f;
    original.alpha_cutoff       = 0.6f;
    original.blend_mode         = MaterialBlendMode::MASK;
    original.cull_mode          = GPUCullMode::NONE;
    original.depth_write        = false;
    original.depth_test         = true;

    // Save to .material USDA via AssetSaverAPI
    bool save_ok = MaterialAsset::saver().save(&original, mat_path.c_str());
    REQUIRE(save_ok);

    // Load via FileLoader and MaterialAsset::loader()
    FileLoader loader(FSLoader::NATIVE);
    loader.mount("/", temp_dir.c_str(), 0);

    auto loader_api = MaterialAsset::loader();
    auto loaded     = static_cast<MaterialAsset*>(loader_api.load(&loader, "/test.material"));

    REQUIRE_NE(loaded, nullptr);

    CHECK_EQ(loaded->base_color_factor.x, doctest::Approx(original.base_color_factor.x));
    CHECK_EQ(loaded->base_color_factor.y, doctest::Approx(original.base_color_factor.y));
    CHECK_EQ(loaded->base_color_factor.z, doctest::Approx(original.base_color_factor.z));
    CHECK_EQ(loaded->base_color_factor.w, doctest::Approx(original.base_color_factor.w));

    CHECK_EQ(loaded->metallic_factor, doctest::Approx(original.metallic_factor));
    CHECK_EQ(loaded->roughness_factor, doctest::Approx(original.roughness_factor));

    CHECK_EQ(loaded->emissive_factor.x, doctest::Approx(original.emissive_factor.x));
    CHECK_EQ(loaded->emissive_factor.y, doctest::Approx(original.emissive_factor.y));
    CHECK_EQ(loaded->emissive_factor.z, doctest::Approx(original.emissive_factor.z));

    CHECK_EQ(loaded->occlusion_strength, doctest::Approx(original.occlusion_strength));
    CHECK_EQ(loaded->normal_scale, doctest::Approx(original.normal_scale));
    CHECK_EQ(loaded->alpha_cutoff, doctest::Approx(original.alpha_cutoff));

    CHECK_EQ(loaded->blend_mode, original.blend_mode);
    CHECK_EQ(loaded->cull_mode, original.cull_mode);
    CHECK_EQ(loaded->depth_write, original.depth_write);
    CHECK_EQ(loaded->depth_test, original.depth_test);

    loader_api.unload(loaded);
}

TEST_CASE("model::model_usd_serialization")
{
    auto temp_dir = fs::temp_directory_path() / "lyra_model_usd_test";
    fs::create_directories(temp_dir);

    auto model_path = temp_dir / "test.model";

    ModelAsset original;
    original.root = 0;

    ModelAsset::Node root_node;
    root_node.name      = "RootNode";
    root_node.transform = glm::translate(Matrix4x4(1.0f), Vector3(1.0f, 2.0f, 3.0f));
    root_node.children.push_back(1);

    ModelAsset::Node child_node;
    child_node.name      = "ChildNode";
    child_node.transform = glm::scale(Matrix4x4(1.0f), Vector3(2.0f, 2.0f, 2.0f));

    original.nodes.push_back(root_node);
    original.nodes.push_back(child_node);

    bool model_save_ok = ModelAsset::saver().save(&original, model_path.c_str());
    REQUIRE(model_save_ok);

    FileLoader loader(FSLoader::NATIVE);
    loader.mount("/", temp_dir.c_str(), 0);

    auto loader_api = ModelAsset::loader();
    auto loaded     = static_cast<ModelAsset*>(loader_api.load(&loader, "/test.model"));

    REQUIRE_NE(loaded, nullptr);
    REQUIRE_EQ(loaded->nodes.size(), 2);
    CHECK_EQ(loaded->nodes[0].name, "RootNode");
    CHECK_EQ(loaded->nodes[1].name, "ChildNode");
    CHECK_EQ(loaded->nodes[0].children.size(), 1);
    CHECK_EQ(loaded->nodes[0].children[0], 1);

    loader_api.unload(loaded);
}

TEST_CASE("scene::scene_usd_serialization")
{
    auto temp_dir = fs::temp_directory_path() / "lyra_scene_usd_test";
    fs::create_directories(temp_dir);

    auto scene_path = temp_dir / "test.scene";

    SceneAsset original;
    original.root = 0;

    SceneAsset::Node root_node;
    root_node.name      = "SceneRoot";
    root_node.transform = Matrix4x4(1.0f);
    root_node.children.push_back(1);

    SceneAsset::Node entity_node;
    entity_node.name      = "Entity1";
    entity_node.transform = glm::translate(Matrix4x4(1.0f), Vector3(5.0f, 0.0f, 0.0f));

    original.nodes.push_back(root_node);
    original.nodes.push_back(entity_node);

    bool scene_save_ok = SceneAsset::saver().save(&original, scene_path.c_str());
    REQUIRE(scene_save_ok);

    FileLoader loader(FSLoader::NATIVE);
    loader.mount("/", temp_dir.c_str(), 0);

    auto loader_api = SceneAsset::loader();
    auto loaded     = static_cast<SceneAsset*>(loader_api.load(&loader, "/test.scene"));

    REQUIRE_NE(loaded, nullptr);
    REQUIRE_EQ(loaded->nodes.size(), 2);
    CHECK_EQ(loaded->nodes[0].name, "SceneRoot");
    CHECK_EQ(loaded->nodes[1].name, "Entity1");
    CHECK_EQ(loaded->nodes[0].children.size(), 1);
    CHECK_EQ(loaded->nodes[0].children[0], 1);

    loader_api.unload(loaded);
}

TEST_CASE("ams::asset_saver_api")
{
    CHECK_NE(MaterialAsset::saver().save, nullptr);
    CHECK_NE(ModelAsset::saver().save, nullptr);
    CHECK_NE(SceneAsset::saver().save, nullptr);
    CHECK_NE(MeshAsset::saver().save, nullptr);

    auto temp_dir = fs::temp_directory_path() / "lyra_ams_saver_test";
    fs::create_directories(temp_dir);

    MaterialAsset mat;
    mat.roughness_factor = 0.42f;
    auto mat_file        = temp_dir / "saver_test.material";
    auto mat_saver       = MaterialAsset::saver();
    bool save_ok1        = mat_saver.save(&mat, mat_file.c_str());
    REQUIRE(save_ok1);
    REQUIRE(fs::exists(mat_file));

    auto caches_dir = temp_dir / "caches";
    fs::create_directories(caches_dir);

    auto registry_file = temp_dir / "Assets.toml";

    AMSDescriptor desc;
    desc.importer.assets_path = temp_dir.c_str();
    desc.importer.caches_path = caches_dir.c_str();
    desc.registry             = registry_file.c_str();
    AssetServer server(desc);
    server.register_asset<MaterialAsset>();
    server.register_asset<ModelAsset>();
    server.register_asset<SceneAsset>();
    server.register_asset<MeshAsset>();

    auto server_save_file = temp_dir / "server_saved.material";
    bool save_ok2         = server.save_asset(mat, server_save_file.c_str());
    REQUIRE(save_ok2);
    REQUIRE(fs::exists(server_save_file));
}

TEST_CASE("ams::text_based_assets_saver")
{
    CHECK_NE(TextAsset::saver().save, nullptr);
    CHECK_NE(TomlAsset::saver().save, nullptr);
    CHECK_NE(JsonAsset::saver().save, nullptr);

    auto temp_dir = fs::temp_directory_path() / "lyra_text_saver_test";
    fs::create_directories(temp_dir);

    // 1. TextAsset
    {
        TextAsset text_orig;
        text_orig.content = "Hello, Lyra Engine!";
        auto text_file = temp_dir / "test.txt";
        bool save_ok = TextAsset::saver().save(&text_orig, text_file.c_str());
        REQUIRE(save_ok);
        REQUIRE(fs::exists(text_file));

        FileLoader loader(FSLoader::NATIVE);
        loader.mount("/", temp_dir.c_str(), 0);
        auto loaded = static_cast<TextAsset*>(TextAsset::loader().load(&loader, "/test.txt"));
        REQUIRE_NE(loaded, nullptr);
        CHECK_EQ(loaded->content, "Hello, Lyra Engine!");
        TextAsset::loader().unload(loaded);
    }

    // 2. JsonAsset
    {
        JsonAsset json_orig;
        json_orig.content = JSON::object({{"name", "Lyra"}, {"version", 1}});
        auto json_file = temp_dir / "test.json";
        bool save_ok = JsonAsset::saver().save(&json_orig, json_file.c_str());
        REQUIRE(save_ok);
        REQUIRE(fs::exists(json_file));

        FileLoader loader(FSLoader::NATIVE);
        loader.mount("/", temp_dir.c_str(), 0);
        auto loaded = static_cast<JsonAsset*>(JsonAsset::loader().load(&loader, "/test.json"));
        REQUIRE_NE(loaded, nullptr);
        CHECK_EQ(loaded->content["name"].get<std::string>(), "Lyra");
        CHECK_EQ(loaded->content["version"].get<int>(), 1);
        JsonAsset::loader().unload(loaded);
    }

    // 3. TomlAsset
    {
        TomlAsset toml_orig;
        toml_orig.content.insert_or_assign("engine", "Lyra");
        toml_orig.content.insert_or_assign("stars", 42);
        auto toml_file = temp_dir / "test.toml";
        bool save_ok = TomlAsset::saver().save(&toml_orig, toml_file.c_str());
        REQUIRE(save_ok);
        REQUIRE(fs::exists(toml_file));

        FileLoader loader(FSLoader::NATIVE);
        loader.mount("/", temp_dir.c_str(), 0);
        auto loaded = static_cast<TomlAsset*>(TomlAsset::loader().load(&loader, "/test.toml"));
        REQUIRE_NE(loaded, nullptr);
        CHECK_EQ(loaded->content["engine"].value_exact<std::string_view>(), "Lyra");
        CHECK_EQ(loaded->content["stars"].value_exact<int64_t>(), 42);
        TomlAsset::loader().unload(loaded);
    }

    // 4. AssetServer save_asset integration
    {
        auto caches_dir = temp_dir / "caches";
        fs::create_directories(caches_dir);
        auto registry_file = temp_dir / "Assets.toml";

        AMSDescriptor desc;
        desc.importer.assets_path = temp_dir.c_str();
        desc.importer.caches_path = caches_dir.c_str();
        desc.registry             = registry_file.c_str();
        AssetServer server(desc);
        server.register_asset<TextAsset>();
        server.register_asset<JsonAsset>();
        server.register_asset<TomlAsset>();

        TextAsset text;
        text.content = "Server saved text";
        auto server_text_file = temp_dir / "server_text.txt";
        bool ok = server.save_asset(text, server_text_file.c_str());
        REQUIRE(ok);
        REQUIRE(fs::exists(server_text_file));
    }
}

