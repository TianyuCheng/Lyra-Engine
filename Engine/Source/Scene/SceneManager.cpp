#include <Lyra/Scene/SceneManager.h>
#include <Lyra/Scene/Mesh.h>

using namespace lyra;

SceneManager::SceneManager(World& world, SceneTree& hierarchy, AssetServer& ams)
    : world(world), hierarchy(hierarchy), ams(ams)
{
}

SceneNode SceneManager::spawn(ModelAssetHandle model, const SpawnParams& params)
{
    if (!model.valid()) {
        return SceneNode{};
    }

    const auto* asset = ams.get_asset(model);
    if (!asset) {
        return SceneNode{};
    }

    return spawn_model_internal(*asset, params);
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

    // create entities and populate components
    for (size_t i = 0; i < scene.nodes.size(); ++i) {
        const auto& src_node = scene.nodes[i];
        Entity      entity   = world.create(src_node.name);
        created_nodes[i]     = SceneNode(entity);

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

        // if this node references a model asset prefab, instantiate it as a child
        if (src_node.model.valid()) {
            const auto* model_asset = ams.get_asset(src_node.model);
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

    // attach root node to parent if specified
    if (params.parent.entity != entt::null && scene.root < scene.nodes.size()) {
        world.add_child(params.parent, created_nodes[scene.root]);
    }

    return (scene.root < scene.nodes.size()) ? created_nodes[scene.root] : SceneNode{};
}

SceneInstanceID SceneManager::load(
    SceneAssetHandle handle,
    LoadMode         mode)
{
    return load(handle, SpawnParams{}, mode);
}

SceneInstanceID SceneManager::load(
    SceneAssetHandle   handle,
    const SpawnParams& params,
    LoadMode           mode)
{
    if (!handle.valid()) {
        return INVALID_SCENE_INSTANCE;
    }

    const auto* asset = ams.get_asset(handle);
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

    for (auto& cb : loaded_callbacks) {
        cb(id);
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

        auto* parent = world.registry.try_get<Parent>(entity);
        if (!parent || parent->node.entity == entt::null || !world.registry.valid(parent->node.entity)) {
            roots_to_destroy.push_back(entity);
        }
    }

    for (auto root_entity : roots_to_destroy) {
        world.destroy_tree(SceneNode(root_entity));
    }

    instances.clear();
    active_instance_id = INVALID_SCENE_INSTANCE;
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
        if (auto* name_comp = world.registry.try_get<NodeName>(entity)) {
            sn.name = name_comp->name;
        } else {
            sn.name = "node_" + std::to_string(scene_node_idx);
        }

        // transform
        if (auto* local = world.registry.try_get<TransformLocal>(entity)) {
            Matrix4x4 scale       = glm::scale(Matrix4x4(1.0f), local->scale);
            Matrix4x4 rotation    = glm::mat4_cast(local->rotation);
            Matrix4x4 translation = glm::translate(Matrix4x4(1.0f), local->position);
            sn.transform          = translation * rotation * scale;
        }

        // mesh component
        if (auto* mesh_comp = world.registry.try_get<Mesh>(entity)) {
            sn.mesh     = mesh_comp->mesh;
            sn.material = mesh_comp->material;
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
        const auto*          active        = get_active();
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

const SceneInstance* SceneManager::get_active() const
{
    if (active_instance_id == INVALID_SCENE_INSTANCE) {
        return nullptr;
    }
    return get_instance(active_instance_id);
}

String SceneManager::get_active_name() const
{
    const auto* active = get_active();
    return active ? active->name : "";
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
