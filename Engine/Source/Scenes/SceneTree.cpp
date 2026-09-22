#include <algorithm>

#include <Lyra/Scenes/SceneTree.h>

using namespace lyra;

SceneTree::SceneTree(World& world) : world(world)
{
    // bind signals for incremental updates
    world.registry.on_construct<TransformLocal>().connect<&SceneTree::on_node_constructed>(this);
    world.registry.on_destroy<TransformLocal>().connect<&SceneTree::on_node_destroyed>(this);
    world.registry.on_construct<Parent>().connect<&SceneTree::on_parent_constructed>(this);
    world.registry.on_destroy<Parent>().connect<&SceneTree::on_parent_destroyed>(this);

    // initial rebuild for existing entities
    rebuild();
}

SceneTree::~SceneTree()
{
    // disconnect signals
    world.registry.on_construct<TransformLocal>().disconnect<&SceneTree::on_node_constructed>(this);
    world.registry.on_destroy<TransformLocal>().disconnect<&SceneTree::on_node_destroyed>(this);
    world.registry.on_construct<Parent>().disconnect<&SceneTree::on_parent_constructed>(this);
    world.registry.on_destroy<Parent>().disconnect<&SceneTree::on_parent_destroyed>(this);
}

void SceneTree::rebuild()
{
    nodes.clear();
    roots.clear();
    free_nodes.clear();
    entity_to_node.clear();

    // find all entities with transforms
    for (auto entity : world.registry.view<TransformLocal>()) {
        on_node_constructed(world.registry, entity);
    }

    // established parenting relationships
    for (auto entity : world.registry.view<Parent>()) {
        on_parent_constructed(world.registry, entity);
    }
}

void SceneTree::update()
{
    for (NodeIndex root_idx : roots) {
        update_node_recursive(root_idx, Matrix4x4(1.0f), false);
    }
}

void SceneTree::on_node_constructed(Registry& registry, Entity entity)
{
    NodeIndex node_idx;
    if (!free_nodes.empty()) {
        node_idx = free_nodes.back();
        free_nodes.pop_back();
        nodes[node_idx] = {entity, INVALID_NODE, INVALID_NODE, INVALID_NODE, 0};
    } else {
        node_idx = static_cast<NodeIndex>(nodes.size());
        nodes.push_back({entity, INVALID_NODE, INVALID_NODE, INVALID_NODE, 0});
    }

    entity_to_node[entity] = node_idx;
    roots.push_back(node_idx);
}

void SceneTree::on_node_destroyed(Registry& registry, Entity entity)
{
    NodeIndex node_idx = find_node(entity);
    if (node_idx == INVALID_NODE) return;

    bool is_root = nodes[node_idx].parent == INVALID_NODE;
    detach_from_parent(node_idx);

    // remove from roots if it was there
    if (is_root) {
        auto it = std::find(roots.begin(), roots.end(), node_idx);
        if (it != roots.end())
            roots.erase(it);
    }

    nodes[node_idx] = {};
    entity_to_node.erase(entity);
    free_nodes.push_back(node_idx);
}

void SceneTree::on_parent_constructed(Registry& registry, Entity entity)
{
    NodeIndex node_idx = find_node(entity);
    if (node_idx == INVALID_NODE) return;

    auto&     parent_comp = registry.get<Parent>(entity);
    NodeIndex parent_idx  = find_node(parent_comp.node.entity);

    if (parent_idx != INVALID_NODE) {
        detach_from_parent(node_idx); // remove from roots or old parent
        attach_to_parent(node_idx, parent_idx);
    }
}

void SceneTree::on_parent_destroyed(Registry& registry, Entity entity)
{
    NodeIndex node_idx = find_node(entity);
    if (node_idx == INVALID_NODE) return;

    detach_from_parent(node_idx);
    roots.push_back(node_idx);
}

void SceneTree::attach_to_parent(NodeIndex node_idx, NodeIndex parent_idx)
{
    auto& node                    = nodes[node_idx];
    node.parent                   = parent_idx;
    node.next_sibling             = nodes[parent_idx].first_child;
    nodes[parent_idx].first_child = node_idx;

    // remove from roots
    auto it = std::find(roots.begin(), roots.end(), node_idx);
    if (it != roots.end()) {
        roots.erase(it);
    }
}

void SceneTree::detach_from_parent(NodeIndex node_idx)
{
    auto& node = nodes[node_idx];
    if (node.parent == INVALID_NODE) return;

    auto& parent_node = nodes[node.parent];
    if (parent_node.first_child == node_idx) {
        parent_node.first_child = node.next_sibling;
    } else {
        NodeIndex prev_sibling = parent_node.first_child;
        while (prev_sibling != INVALID_NODE && nodes[prev_sibling].next_sibling != node_idx) {
            prev_sibling = nodes[prev_sibling].next_sibling;
        }
        if (prev_sibling != INVALID_NODE) {
            nodes[prev_sibling].next_sibling = node.next_sibling;
        }
    }

    node.parent       = INVALID_NODE;
    node.next_sibling = INVALID_NODE;
}

void SceneTree::update_node_recursive(NodeIndex node_idx, const Matrix4x4& parent_xform, bool parent_dirty)
{
    auto& node            = nodes[node_idx];
    auto  entity          = node.entity;
    auto& local_transform = world.get_component<TransformLocal>(entity);
    auto& world_transform = world.get_component<TransformWorld>(entity);

    bool is_dirty = parent_dirty || local_transform.flags.contains(TransformFlag::LOCAL_DIRTY);
    if (is_dirty) {
        // mark world dirty for children
        local_transform.flags.set(TransformFlag::WORLD_DIRTY);

        // calculate local transform matrix
        Matrix4x4 scale       = glm::scale(Matrix4x4(1.0f), local_transform.scale);
        Matrix4x4 rotation    = glm::mat4_cast(local_transform.rotation);
        Matrix4x4 translation = glm::translate(Matrix4x4(1.0f), local_transform.position);
        Matrix4x4 local_xform = translation * rotation * scale;

        world_transform.xform = parent_xform * local_xform;

        // clear local dirty flag
        local_transform.flags.unset(TransformFlag::LOCAL_DIRTY);
    } else {
        // ensure world dirty is cleared if we didn't update
        local_transform.flags.unset(TransformFlag::WORLD_DIRTY);
    }

    // traverse children
    NodeIndex child_idx = node.first_child;
    while (child_idx != INVALID_NODE) {
        update_node_recursive(child_idx, world_transform.xform, is_dirty);
        child_idx = nodes[child_idx].next_sibling;
    }
}
