#include <fstream>
#include <filesystem>
#include <Lyra/Scene/Mesh.h>
#include <Lyra/Scene/SceneManager.h>
#include <Lyra/Assets/Format/SceneAsset.h>
#include <Lyra/Assets/Format/ModelAsset.h>
#include <Lyra/Assets/AMSServer.h>
#include <Lyra/JobSystem/JobSystem.h>
#include "helper.h"

TEST_CASE("scn::basic_scene_graph" * doctest::description("Basic Scene Graph"))
{
    lyra::World world;

    SUBCASE("entity creation")
    {
        auto entity = world.create();

        CHECK(world.registry.valid(entity));
        CHECK(world.registry.all_of<lyra::TransformLocal>(entity));
        CHECK(world.registry.all_of<lyra::TransformWorld>(entity));
    }

    SUBCASE("child management")
    {
        auto parent = world.create();
        auto child  = world.create();

        world.add_child(parent, child);

        CHECK_EQ(world.registry.get<lyra::Parent>(child).node.entity, parent);
        {
            auto& children = world.registry.get<lyra::Children>(parent).nodes;
            bool  found    = false;
            for (auto c : children) {
                if (c.entity == child) {
                    found = true;
                    break;
                }
            }
            CHECK(found);
        }

        world.del_child(parent, child);

        CHECK(!world.registry.any_of<lyra::Parent>(child));
        {
            auto& children = world.registry.get<lyra::Children>(parent).nodes;
            bool  found    = false;
            for (auto c : children) {
                if (c.entity == child) {
                    found = true;
                    break;
                }
            }
            CHECK(!found);
        }
    }

    SUBCASE("transform manipulation")
    {
        auto  node  = world.create();
        auto& local = world.registry.get<lyra::TransformLocal>(node);

        CHECK_EQ(local.position, lyra::Vector3(0.0f));
        CHECK_EQ(local.scale, lyra::Vector3(1.0f));
        CHECK_EQ(local.rotation.w, 1.0f);
        CHECK_EQ(local.flags, lyra::TransformFlag::NONE);

        world.translate(node, {1.0f, 2.0f, 3.0f});
        CHECK_EQ(world.registry.get<lyra::TransformLocal>(node).position, lyra::Vector3(1.0f, 2.0f, 3.0f));
        CHECK_EQ((world.registry.get<lyra::TransformLocal>(node).flags & lyra::TransformFlag::LOCAL_DIRTY), lyra::TransformFlag::LOCAL_DIRTY);

        world.scale(node, {2.0f, 2.0f, 2.0f});
        CHECK_EQ(world.registry.get<lyra::TransformLocal>(node).scale, lyra::Vector3(2.0f, 2.0f, 2.0f));

        world.rotate(node, {0.0f, 1.0f, 0.0f}, 90.0f);
        auto& final_local = world.registry.get<lyra::TransformLocal>(node);
        auto  expected_q  = glm::angleAxis(glm::radians(90.0f), lyra::Vector3(0.0f, 1.0f, 0.0f));
        CHECK_EQ(final_local.rotation.w, doctest::Approx(expected_q.w));
        CHECK_EQ(final_local.rotation.x, doctest::Approx(expected_q.x));
        CHECK_EQ(final_local.rotation.y, doctest::Approx(expected_q.y));
        CHECK_EQ(final_local.rotation.z, doctest::Approx(expected_q.z));
    }

    SUBCASE("hierarchy and transform propagation")
    {
        auto parent = world.create();
        auto child  = world.create();
        world.add_child(parent, child);

        lyra::SceneTree hierarchy(world);
        hierarchy.rebuild();

        CHECK_EQ(hierarchy.size(), 2);

        world.translate(parent, {10.0f, 0.0f, 0.0f});
        world.translate(child, {0.0f, 5.0f, 0.0f});
        hierarchy.update();

        auto& parent_world = world.registry.get<lyra::TransformWorld>(parent);
        auto& child_world  = world.registry.get<lyra::TransformWorld>(child);

        // check parent translation (10, 0, 0)
        CHECK_EQ(parent_world.xform[3][0], doctest::Approx(10.0f));
        CHECK_EQ(parent_world.xform[3][1], doctest::Approx(0.0f));
        CHECK_EQ(parent_world.xform[3][2], doctest::Approx(0.0f));

        // check child translation (10, 5, 0) - inherited from parent
        CHECK_EQ(child_world.xform[3][0], doctest::Approx(10.0f));
        CHECK_EQ(child_world.xform[3][1], doctest::Approx(5.0f));
        CHECK_EQ(child_world.xform[3][2], doctest::Approx(0.0f));
    }

    SUBCASE("destroy_tree recursive destruction")
    {
        auto root       = world.create("root");
        auto child      = world.create("child");
        auto grandchild = world.create("grandchild");

        world.add_child(root, child);
        world.add_child(child, grandchild);

        CHECK(world.registry.valid(root));
        CHECK(world.registry.valid(child));
        CHECK(world.registry.valid(grandchild));

        world.destroy_tree(root);

        CHECK(!world.registry.valid(root));
        CHECK(!world.registry.valid(child));
        CHECK(!world.registry.valid(grandchild));
    }
}

struct SceneSuiteEnv
{
    std::filesystem::path        temp_dir;
    lyra::Own<lyra::FileLoader>  loader;
    lyra::Own<lyra::AssetServer> ams;

    SceneSuiteEnv()
    {
        if (!lyra::JobScheduler::is_initialized()) {
            lyra::JobScheduler::init();
        }

        temp_dir = std::filesystem::temp_directory_path() / "lyra_scn_mgr_test";
        std::filesystem::remove_all(temp_dir);
        std::filesystem::create_directories(temp_dir);

        // 1. prepare model asset
        {
            lyra::ModelAsset model;
            model.root = 0;

            // node 0: root
            lyra::ModelAsset::Node root_node;
            root_node.name      = "CarRoot";
            root_node.transform = glm::translate(lyra::Matrix4x4(1.0f), lyra::Vector3(1.0f, 2.0f, 3.0f));
            root_node.children  = {1};
            model.nodes.push_back(root_node);

            // node 1: child mesh
            lyra::ModelAsset::Node wheel_node;
            wheel_node.name      = "WheelFL";
            wheel_node.transform = glm::translate(lyra::Matrix4x4(1.0f), lyra::Vector3(0.0f, -0.5f, 1.0f));
            wheel_node.mesh      = lyra::MeshAssetHandle(42);
            wheel_node.material  = lyra::MaterialAssetHandle(99);
            model.nodes.push_back(wheel_node);

            auto model_path = temp_dir / "car.model";
            lyra::ModelAsset::saver().save(&model, model_path.c_str());

            lyra::JSON meta;
            meta["guid"] = 5001;
            meta["type"] = to_string(lyra::ModelAsset::type);
            std::ofstream f(temp_dir / "car.model.import");
            f << meta.dump();
        }

        // 2. prepare scene assets
        {
            lyra::SceneAsset scene_a;
            scene_a.root = 0;
            lyra::SceneAsset::Node a_root;
            a_root.name = "SceneA_Root";
            scene_a.nodes.push_back(a_root);

            auto a_path = temp_dir / "scene_a.scene";
            lyra::SceneAsset::saver().save(&scene_a, a_path.c_str());
            {
                lyra::JSON meta;
                meta["guid"] = 6001;
                meta["type"] = to_string(lyra::SceneAsset::type);
                std::ofstream f(temp_dir / "scene_a.scene.import");
                f << meta.dump();
            }

            lyra::SceneAsset scene_b;
            scene_b.root = 0;
            lyra::SceneAsset::Node b_root;
            b_root.name = "SceneB_Root";
            scene_b.nodes.push_back(b_root);

            auto b_path = temp_dir / "scene_b.scene";
            lyra::SceneAsset::saver().save(&scene_b, b_path.c_str());
            {
                lyra::JSON meta;
                meta["guid"] = 6002;
                meta["type"] = to_string(lyra::SceneAsset::type);
                std::ofstream f(temp_dir / "scene_b.scene.import");
                f << meta.dump();
            }
        }

        auto reg_file = temp_dir / "Assets.toml";
        loader = std::make_unique<lyra::FileLoader>(lyra::FSLoader::NATIVE);
        loader->mount("/", temp_dir.c_str(), 0);

        lyra::AMSDescriptor desc = {};
        desc.registry             = reg_file.c_str();
        desc.loader.assets        = loader.get();
        desc.importer.assets_path = temp_dir.c_str();

        ams = std::make_unique<lyra::AssetServer>(desc);
        ams->register_asset<lyra::ModelAsset>();
        ams->register_asset<lyra::SceneAsset>();
    }

    ~SceneSuiteEnv()
    {
        ams.reset();
        loader.reset();
        std::filesystem::remove_all(temp_dir);
        if (lyra::JobScheduler::is_initialized()) {
            lyra::JobScheduler::shutdown();
        }
    }

    static SceneSuiteEnv& get()
    {
        static SceneSuiteEnv env;
        return env;
    }
};

TEST_CASE("scn::scene_manager" * doctest::description("Scene Manager Lifecycle and Spawning"))
{
    auto& env = SceneSuiteEnv::get();

    lyra::World        world;
    lyra::SceneTree    hierarchy(world);
    lyra::SceneManager scene_manager(world, hierarchy, *env.ams);

    SUBCASE("spawn model asset handle")
    {
        auto model_handle = env.ams->load_asset<lyra::ModelAsset>("car.model");
        REQUIRE(model_handle.valid());

        int timeout = 100;
        while (env.ams->get_asset(model_handle) == nullptr && timeout-- > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        REQUIRE_NE(env.ams->get_asset(model_handle), nullptr);

        lyra::SpawnParams params;
        params.position = lyra::Vector3(10.0f, 0.0f, 0.0f);

        auto root = scene_manager.spawn(model_handle, params);

        CHECK(world.registry.valid(root.entity));
        CHECK_EQ(world.registry.get<lyra::NodeName>(root.entity).name, "CarRoot");

        auto& root_local = world.registry.get<lyra::TransformLocal>(root.entity);
        CHECK_EQ(root_local.position.x, doctest::Approx(11.0f)); // 1.0 + 10.0
        CHECK_EQ(root_local.position.y, doctest::Approx(2.0f));
        CHECK_EQ(root_local.position.z, doctest::Approx(3.0f));

        // check child exists
        auto* children = world.registry.try_get<lyra::Children>(root.entity);
        REQUIRE_NE(children, nullptr);
        REQUIRE_EQ(children->nodes.size(), 1);

        auto child_node = children->nodes[0];
        CHECK_EQ(world.registry.get<lyra::NodeName>(child_node.entity).name, "WheelFL");

        auto* mesh_comp = world.registry.try_get<lyra::Mesh>(child_node.entity);
        REQUIRE_NE(mesh_comp, nullptr);
        CHECK_EQ(mesh_comp->mesh.guid, 42);
        CHECK_EQ(mesh_comp->material.guid, 99);

        // update hierarchy transforms
        scene_manager.update();

        auto& child_world = world.registry.get<lyra::TransformWorld>(child_node.entity);
        CHECK_EQ(child_world.xform[3][0], doctest::Approx(11.0f));
        CHECK_EQ(child_world.xform[3][1], doctest::Approx(1.5f));
        CHECK_EQ(child_world.xform[3][2], doctest::Approx(4.0f));
    }

    SUBCASE("load and additive load scene handle")
    {
        auto handle_a = env.ams->load_asset<lyra::SceneAsset>("scene_a.scene");
        auto handle_b = env.ams->load_asset<lyra::SceneAsset>("scene_b.scene");
        REQUIRE(handle_a.valid());
        REQUIRE(handle_b.valid());

        int timeout = 100;
        while ((env.ams->get_asset(handle_a) == nullptr || env.ams->get_asset(handle_b) == nullptr) && timeout-- > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        REQUIRE_NE(env.ams->get_asset(handle_a), nullptr);
        REQUIRE_NE(env.ams->get_asset(handle_b), nullptr);

        auto id_a = scene_manager.load(handle_a, lyra::LoadMode::SINGLE);
        CHECK_NE(id_a, lyra::INVALID_SCENE_INSTANCE);
        CHECK_EQ(scene_manager.get_active()->id, id_a);

        auto id_b = scene_manager.load(handle_b, lyra::LoadMode::ADDITIVE);
        CHECK_NE(id_b, lyra::INVALID_SCENE_INSTANCE);

        // both instances should exist in the world
        CHECK_NE(scene_manager.get_instance(id_a), nullptr);
        CHECK_NE(scene_manager.get_instance(id_b), nullptr);

        auto root_a = scene_manager.get_instance(id_a)->root;
        auto root_b = scene_manager.get_instance(id_b)->root;
        CHECK(world.registry.valid(root_a.entity));
        CHECK(world.registry.valid(root_b.entity));

        // unload scene_b
        bool unloaded = scene_manager.unload(id_b);
        CHECK(unloaded);
        CHECK_EQ(scene_manager.get_instance(id_b), nullptr);
        CHECK(!world.registry.valid(root_b.entity));
        CHECK(world.registry.valid(root_a.entity));
    }

    SUBCASE("clear with DontDestroyOnLoad")
    {
        auto persistent_ent = world.create("PersistentEntity");
        world.registry.emplace<lyra::DontDestroyOnLoad>(persistent_ent);

        auto normal_ent = world.create("TransientEntity");

        CHECK(world.registry.valid(persistent_ent));
        CHECK(world.registry.valid(normal_ent));

        scene_manager.clear();

        CHECK(world.registry.valid(persistent_ent));
        CHECK(!world.registry.valid(normal_ent));
    }

    SUBCASE("serialize scene")
    {
        scene_manager.clear();

        auto root = world.create("SerializedRoot");
        auto child = world.create("SerializedChild");
        world.add_child(root, child);

        world.translate(child, {0.0f, 10.0f, 0.0f});
        world.registry.emplace<lyra::Mesh>(child, lyra::MeshAssetHandle(55), lyra::MaterialAssetHandle(66));

        hierarchy.rebuild();

        auto serialized = scene_manager.serialize();
        REQUIRE_GE(serialized.nodes.size(), 2);

        bool found_child = false;
        for (const auto& n : serialized.nodes) {
            if (n.name == "SerializedChild") {
                found_child = true;
                CHECK_EQ(n.mesh.guid, 55);
                CHECK_EQ(n.material.guid, 66);
                CHECK_EQ(n.transform[3][1], doctest::Approx(10.0f));
            }
        }
        CHECK(found_child);
    }
}


