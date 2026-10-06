#include <fstream>
#include <Lyra/Utilities/GUID.h>
#include <Lyra/Utilities/Logger.h>
#include <Lyra/JobSystem/Jobs.h>
#include <Lyra/Assets/Format/SceneAsset.h>
#include <Lyra/Scene/SceneManager.h>
#include <Lyra/Scene/Mesh.h>
#include <Lyra/Scene/Camera.h>

using namespace lyra;

SceneManager::SceneManager(World& world, SceneTree& hierarchy, AssetServer& ams)
    : world(world), hierarchy(hierarchy), ams(ams)
{
    // do nothing
}

static void split_name_index(const String& name, String& out_prefix, uint& out_index)
{
    if (name.size() >= 4 && name.back() == ')') {
        auto open_paren = name.rfind(" (");
        if (open_paren != String::npos && open_paren + 2 < name.size() - 1) {
            bool all_digits = true;
            for (size_t i = open_paren + 2; i < name.size() - 1; ++i) {
                if (!std::isdigit(static_cast<unsigned char>(name[i]))) {
                    all_digits = false;
                    break;
                }
            }
            if (all_digits) {
                out_prefix = name.substr(0, open_paren);
                try {
                    out_index = static_cast<uint>(std::stoul(name.substr(open_paren + 2, name.size() - open_paren - 3)));
                    return;
                } catch (const std::exception& e) {
                    spdlog::warn("SceneManager: Failed to parse index in name '{}': {}", name, e.what());
                    out_index = 0;
                }
            }
        }
    }
    out_prefix = name;
    out_index  = 0;
}

String SceneManager::resolve_unique_name(SceneNode parent, const String& base_name) const
{
    if (base_name.empty()) {
        return "Node";
    }

    SceneNode effective_parent = parent;
    if (effective_parent.entity == entt::null) {
        if (const auto* active = get_active()) {
            if (active->root.entity != entt::null) {
                effective_parent = active->root;
            }
        }
    }

    HashSet<String> existing_names;

    if (effective_parent.entity != entt::null && world.registry.valid(effective_parent.entity)) {
        if (auto* children = world.registry.try_get<Children>(effective_parent.entity)) {
            for (auto child : children->nodes) {
                if (world.registry.valid(child.entity)) {
                    if (auto* name_comp = world.registry.try_get<NodeName>(child.entity)) {
                        existing_names.insert(name_comp->name);
                    }
                }
            }
        }
    } else {
        auto view = world.registry.view<NodeName>();
        for (auto entity : view) {
            if (!world.registry.any_of<Parent>(entity)) {
                existing_names.insert(view.get<NodeName>(entity).name);
            }
        }
    }

    if (existing_names.find(base_name) == existing_names.end()) {
        return base_name;
    }

    String prefix;
    uint   start_index = 0;
    split_name_index(base_name, prefix, start_index);

    uint index = (start_index > 0) ? (start_index + 1) : 1;
    while (true) {
        String candidate = prefix + " (" + std::to_string(index) + ")";
        if (existing_names.find(candidate) == existing_names.end()) {
            return candidate;
        }
        ++index;
    }
}

SceneNode SceneManager::spawn(ModelAssetHandle model, const SpawnParams& params)
{
    if (!model.valid()) {
        return SceneNode{};
    }

    auto asset = ams.get_asset(model);
    if (!asset) {
        JobScheduler::wait_idle();
        asset = ams.get_asset(model);
    }

    if (!asset) {
        return SceneNode{};
    }

    SpawnParams effective_params = params;
    if (effective_params.name.empty()) {
        if (asset->root < asset->nodes.size() && !asset->nodes[asset->root].name.empty()) {
            effective_params.name = asset->nodes[asset->root].name;
        } else {
            StringView asset_path = ams.get_path(model.guid);
            if (!asset_path.empty()) {
                String s(asset_path);
                auto   hash_pos = s.find('#');
                if (hash_pos != String::npos) {
                    s = s.substr(0, hash_pos);
                }
                effective_params.name = Path(s).stem().string();
            }
        }
    }

    return spawn_model_internal(*asset, effective_params);
}

SceneNode SceneManager::spawn_model_internal(
    const ModelAsset&  model,
    const SpawnParams& params)
{
    SceneAsset scene = SceneAsset::from_model(model);
    return spawn_scene_internal(scene, params);
}

SceneNode SceneManager::spawn_scene_internal(
    const SceneAsset&  scene,
    const SpawnParams& params)
{
    if (scene.nodes.empty()) {
        return SceneNode{};
    }

    Vector<SceneNode> created_nodes(scene.nodes.size());

    // determine unique name for the root node
    String root_name = params.name;
    if (root_name.empty() && scene.root < scene.nodes.size()) {
        root_name = scene.nodes[scene.root].name;
    }
    if (root_name.empty()) {
        root_name = "Node";
    }

    SceneNode target_parent = params.parent;
    if (target_parent.entity == entt::null) {
        if (const auto* active = get_active()) {
            if (active->root.entity != entt::null) {
                target_parent = active->root;
            }
        }
    }
    root_name = resolve_unique_name(target_parent, root_name);

    // create entities and populate components
    for (size_t i = 0; i < scene.nodes.size(); ++i) {
        const auto& src_node  = scene.nodes[i];
        String      node_name = (i == scene.root) ? root_name : src_node.name;
        Entity      entity    = world.create(node_name);
        created_nodes[i]      = SceneNode(entity);

        // decompose local transform
        Vector3    pos;
        Quaternion rot;
        Vector3    scl;
        decompose_transform(src_node.transform, pos, rot, scl);

        // apply SpawnParams offset to root node
        if (i == scene.root) {
            pos += params.position;
            rot = params.rotation * rot;
            scl *= params.scale;
        }

        auto& local    = world.registry.get<TransformLocal>(entity);
        local.position = pos;
        local.rotation = rot;
        local.scale    = scl;
        local.flags.set(TransformFlag::LOCAL_DIRTY);

        // attach mesh component if mesh reference is valid
        if (src_node.mesh.valid()) {
            world.registry.emplace<Mesh>(entity, src_node.mesh, src_node.material);
        }

        // attach camera component if present
        if (src_node.has_camera || src_node.name == "Main Camera") {
            world.registry.emplace_or_replace<Camera>(entity);
        }

        // if this node references a model asset prefab, instantiate it as a child
        if (src_node.model.valid()) {
            const auto model_asset = ams.get_asset(src_node.model);
            if (model_asset) {
                SpawnParams sub_params;
                sub_params.parent = created_nodes[i];
                spawn_model_internal(*model_asset, sub_params);
            }
        }
    }

    // establish parent-child hierarchy
    for (size_t i = 0; i < scene.nodes.size(); ++i) {
        for (uint child_idx : scene.nodes[i].children) {
            if (child_idx < scene.nodes.size()) {
                world.add_child(created_nodes[i], created_nodes[child_idx]);
            }
        }
    }

    // attach root node to parent if specified, or active scene root
    if (scene.root < scene.nodes.size()) {
        if (params.parent.entity != entt::null) {
            world.add_child(params.parent, created_nodes[scene.root]);
        } else if (const auto active = get_active()) {
            if (active->root.entity != entt::null && active->root.entity != created_nodes[scene.root].entity) {
                world.add_child(active->root, created_nodes[scene.root]);
            }
        }
    }

    dirty = true;

    hierarchy.update();

    return (scene.root < scene.nodes.size()) ? created_nodes[scene.root] : SceneNode{};
}

SceneInstanceID SceneManager::create(const String& name)
{
    clear();

    Entity    root_entity = world.create(name);
    SceneNode root_node(root_entity);

    // default scene contains one "Main Camera"
    Entity    cam_entity = world.create("Main Camera");
    SceneNode cam_node(cam_entity);
    world.translate(cam_node, {0.0f, 1.0f, 8.0f});
    world.rotate(cam_node, {1.0f, 0.0f, 0.0f}, -20.0f);
    world.add_component<Camera>(cam_node);
    world.add_child(root_node, cam_node);

    SceneInstanceID id = next_instance_id++;
    SceneInstance   instance;
    instance.id        = id;
    instance.name      = name;
    instance.root      = root_node;
    instance.is_active = true;

    instances[id]      = instance;
    active_instance_id = id;
    dirty              = false;

    hierarchy.update();

    for (auto& cb : loaded_callbacks) {
        cb(id);
    }

    return id;
}

SceneInstanceID SceneManager::load(SceneAssetHandle handle, LoadMode mode)
{
    return load(handle, SpawnParams{}, mode);
}

SceneInstanceID SceneManager::load(SceneAssetHandle handle, const SpawnParams& params, LoadMode mode)
{
    if (!handle.valid()) {
        return INVALID_SCENE_INSTANCE;
    }

    auto asset = ams.get_asset(handle);
    if (!asset) {
        JobScheduler::wait_idle();
        asset = ams.get_asset(handle);
    }
    if (!asset) {
        return INVALID_SCENE_INSTANCE;
    }

    if (mode == LoadMode::SINGLE) {
        clear();
    }

    SceneNode root = spawn_scene_internal(*asset, params);
    if (root.entity == entt::null) {
        return INVALID_SCENE_INSTANCE;
    }

    SceneInstanceID id = next_instance_id++;
    SceneInstance   instance;
    instance.id        = id;
    instance.handle    = handle;
    instance.root      = root;
    instance.is_active = (mode == LoadMode::SINGLE);

    if (mode == LoadMode::SINGLE) {
        if (active_instance_id != INVALID_SCENE_INSTANCE) {
            auto old_it = instances.find(active_instance_id);
            if (old_it != instances.end()) {
                old_it->second.is_active = false;
            }
        }
        active_instance_id = id;
    }

    instances[id] = instance;

    hierarchy.update();

    for (auto& cb : loaded_callbacks) {
        cb(id);
    }

    return id;
}

SceneInstanceID SceneManager::load(const Path& path, LoadMode mode)
{
    if (!fs::exists(path)) {
        return INVALID_SCENE_INSTANCE;
    }

    // check for .import file to read GUID
    Path    import_path = path.string() + ".import";
    AssetID guid        = 0;

    if (fs::exists(import_path)) {
        std::ifstream f(import_path);
        if (f.is_open()) {
            try {
                JSON meta = JSON::parse(f);
                if (meta.contains("guid")) {
                    guid = meta["guid"].get<AssetID>();
                }
            } catch (...) {
            }
        }
    }

    if (guid == 0) {
        guid = ams.get_guid(path.filename().string().c_str());
    }

    if (guid == 0) {
        guid = random_guid();
        JSON meta;
        meta["guid"] = guid;
        meta["type"] = to_string(SceneAsset::type);

        std::ofstream f(import_path);
        if (f.is_open()) {
            f << meta.dump(4);
            f.close();
        }
    }

    ams.register_asset_entry(guid, path, SceneAsset::type);

    SceneAssetHandle handle = ams.load_asset<SceneAsset>(guid);
    if (!handle.valid()) {
        return INVALID_SCENE_INSTANCE;
    }

    JobScheduler::wait_idle();

    int timeout = 100;
    while (ams.get_asset(handle) == nullptr && timeout-- > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    SceneInstanceID id = load(handle, mode);
    if (id != INVALID_SCENE_INSTANCE) {
        auto it = instances.find(id);
        if (it != instances.end()) {
            it->second.path = path;
            it->second.name = path.stem().string();
        }
        if (mode == LoadMode::SINGLE) {
            dirty = false;
        }
    }

    return id;
}

bool SceneManager::unload(SceneInstanceID instance_id)
{
    auto it = instances.find(instance_id);
    if (it == instances.end()) {
        return false;
    }

    // destroy the root node and all its descendants
    world.destroy_tree(it->second.root);

    if (active_instance_id == instance_id) {
        active_instance_id = INVALID_SCENE_INSTANCE;
    }

    for (auto& cb : unloaded_callbacks) {
        cb(instance_id);
    }

    instances.erase(it);
    return true;
}

void SceneManager::clear()
{
    // collect all root entities that are not marked DontDestroyOnLoad
    Vector<Entity> roots_to_destroy;
    for (auto entity : world.registry.view<TransformLocal>()) {
        if (world.registry.any_of<DontDestroyOnLoad>(entity)) {
            continue;
        }

        auto parent = world.registry.try_get<Parent>(entity);
        if (!parent || parent->node.entity == entt::null || !world.registry.valid(parent->node.entity)) {
            roots_to_destroy.push_back(entity);
        }
    }

    for (auto root_entity : roots_to_destroy) {
        world.destroy_tree(SceneNode(root_entity));
    }

    instances.clear();
    active_instance_id = INVALID_SCENE_INSTANCE;

    hierarchy.update();
}

SceneAsset SceneManager::serialize() const
{
    SceneAsset scene;

    HashMap<Entity, uint> entity_to_index;

    // collect all nodes in traversal order from scene tree roots
    auto collect_node = [&](auto& self, SceneTree::NodeIndex node_idx) -> uint {
        if (node_idx == SceneTree::INVALID_NODE) return static_cast<uint>(-1);

        const auto& tree_node = hierarchy[node_idx];
        Entity      entity    = tree_node.entity;

        uint scene_node_idx     = static_cast<uint>(scene.nodes.size());
        entity_to_index[entity] = scene_node_idx;

        SceneAsset::Node sn;

        // node name
        if (auto name_comp = world.registry.try_get<NodeName>(entity)) {
            sn.name = name_comp->name;
        } else {
            sn.name = "node_" + std::to_string(scene_node_idx);
        }

        // transform
        if (auto local = world.registry.try_get<TransformLocal>(entity)) {
            Matrix4x4 scale       = glm::scale(Matrix4x4(1.0f), local->scale);
            Matrix4x4 rotation    = glm::mat4_cast(local->rotation);
            Matrix4x4 translation = glm::translate(Matrix4x4(1.0f), local->position);
            sn.transform          = translation * rotation * scale;
        }

        // mesh component
        if (auto mesh_comp = world.registry.try_get<Mesh>(entity)) {
            sn.mesh     = mesh_comp->mesh;
            sn.material = mesh_comp->material;
        }

        // camera component
        if (world.registry.any_of<Camera>(entity)) {
            sn.has_camera = true;
        }

        scene.nodes.push_back(sn);

        // traverse children
        auto child_idx = tree_node.first_child;
        while (child_idx != SceneTree::INVALID_NODE) {
            uint child_scene_idx = self(self, child_idx);
            if (child_scene_idx != static_cast<uint>(-1)) {
                scene.nodes[scene_node_idx].children.push_back(child_scene_idx);
            }
            child_idx = hierarchy[child_idx].next_sibling;
        }

        return scene_node_idx;
    };

    if (hierarchy.size() > 0) {
        // use active instance root if available, otherwise first root
        SceneTree::NodeIndex root_tree_idx = SceneTree::INVALID_NODE;
        const auto           active        = get_active();
        if (active && active->root.entity != entt::null) {
            root_tree_idx = hierarchy.find_node(active->root.entity);
        }

        if (root_tree_idx != SceneTree::INVALID_NODE) {
            scene.root = collect_node(collect_node, root_tree_idx);
        } else {
            for (auto root_idx : hierarchy) {
                scene.root = collect_node(collect_node, root_idx);
                break; // serialize first root as main root
            }
        }
    }

    return scene;
}

bool SceneManager::save(const Path& path)
{
    SceneAsset scene = serialize();

    if (path.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
    }

    if (!ams.save_asset(scene, path.c_str())) {
        return false;
    }

    // write or verify .import metadata next to the scene file
    Path    import_path = path.string() + ".import";
    AssetID guid        = 0;
    if (fs::exists(import_path)) {
        std::ifstream f(import_path);
        if (f.is_open()) {
            try {
                JSON meta = JSON::parse(f);
                if (meta.contains("guid")) {
                    guid = meta["guid"].get<AssetID>();
                }
            } catch (...) {
            }
        }
    }

    if (guid == 0) {
        guid = random_guid();
        JSON meta;
        meta["guid"] = guid;
        meta["type"] = to_string(SceneAsset::type);

        std::ofstream f(import_path);
        if (f.is_open()) {
            f << meta.dump(4);
            f.close();
        }
    }

    ams.register_asset_entry(guid, path, SceneAsset::type);

    if (active_instance_id != INVALID_SCENE_INSTANCE) {
        auto it = instances.find(active_instance_id);
        if (it != instances.end()) {
            it->second.path   = path;
            it->second.name   = path.stem().string();
            it->second.handle = SceneAssetHandle(guid);
        }
    }

    dirty = false;
    return true;
}

bool SceneManager::save_active()
{
    const auto active = get_active();
    if (!active || active->path.empty()) {
        return false;
    }
    return save(active->path);
}

const SceneInstance* SceneManager::get_active() const
{
    if (active_instance_id == INVALID_SCENE_INSTANCE) {
        return nullptr;
    }
    return get_instance(active_instance_id);
}

String SceneManager::get_active_name() const
{
    const auto active = get_active();
    return active ? active->name : "";
}

Path SceneManager::get_active_path() const
{
    const auto active = get_active();
    return active ? active->path : Path{};
}

const SceneInstance* SceneManager::get_instance(SceneInstanceID id) const
{
    auto it = instances.find(id);
    return (it != instances.end()) ? &it->second : nullptr;
}

void SceneManager::update()
{
    // update scene tree world transforms
    hierarchy.update();
}
