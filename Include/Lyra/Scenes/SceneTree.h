#pragma once

#ifndef LYRA_LYRA_SCENES_SCENE_TREE_H
#define LYRA_LYRA_SCENES_SCENE_TREE_H

#include <Lyra/Common/Math.h>
#include <Lyra/Common/Macros.h>
#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/Collections.h>

#include <Lyra/Scenes/World.h>

namespace lyra
{

    /**
     * @brief auxiliary tree hierarchy for fast scene traversal and transform updates.
     *
     * this class maintains a tree representation of the scene graph and a cached traversal order.
     * it uses entt signals to stay in sync with the world ecs incrementally.
     */
    class SceneTree
    {
    public:
        using NodeIndex = uint;

        constexpr static NodeIndex INVALID_NODE = NodeIndex(-1);

        struct Node
        {
            Entity    entity;
            NodeIndex parent       = INVALID_NODE;
            NodeIndex first_child  = INVALID_NODE;
            NodeIndex next_sibling = INVALID_NODE;
            uint      depth        = 0;
        };

        explicit SceneTree(World& world);
        virtual ~SceneTree();

        /**
         * @brief manually trigger a reconstruction of the hierarchy from the world.
         */
        void rebuild();

        /**
         * @brief update world transforms of all nodes in the hierarchy.
         *
         * recursively propagates transforms from roots to children.
         */
        void update();

        /**
         * @brief helper to find a node index by entity.
         */
        FORCE_INLINE NodeIndex find_node(Entity entity) const
        {
            auto it = entity_to_node.find(entity);
            return (it != entity_to_node.end()) ? it->second : INVALID_NODE;
        }

        /**
         * @brief helpers for iterator begin
         */
        FORCE_INLINE auto begin() { return roots.begin(); }
        FORCE_INLINE auto begin() const { return roots.begin(); }
        FORCE_INLINE auto cegin() const { return roots.begin(); }
        FORCE_INLINE auto regin() const { return roots.begin(); }

        /**
         * @brief helpers for iterator end
         */
        FORCE_INLINE auto end() { return roots.end(); }
        FORCE_INLINE auto end() const { return roots.end(); }
        FORCE_INLINE auto cend() const { return roots.end(); }
        FORCE_INLINE auto rend() const { return roots.end(); }

        /**
         * @brief helpers to retrieve SceneTree::Node.
         */
        FORCE_INLINE Node&       at(NodeIndex i) { return nodes.at(i); }
        FORCE_INLINE const Node& at(NodeIndex i) const { return nodes.at(i); }
        FORCE_INLINE Node&       operator[](NodeIndex i) { return nodes[i]; }

        /**
         * @brief helper to retrieve total number of nodes in the scene.
         */
        FORCE_INLINE std::size_t size() const { return nodes.size(); }

    private:
        // incremental update helpers
        void on_node_constructed(Registry& registry, Entity entity);
        void on_node_destroyed(Registry& registry, Entity entity);
        void on_parent_constructed(Registry& registry, Entity entity);
        void on_parent_destroyed(Registry& registry, Entity entity);

        void attach_to_parent(NodeIndex node_idx, NodeIndex parent_idx);
        void detach_from_parent(NodeIndex node_idx);

        void update_node_recursive(NodeIndex node_idx, const Matrix4x4& parent_xform, bool parent_dirty);

        World&                     world;
        Vector<Node>               nodes;
        Vector<NodeIndex>          roots;
        Vector<NodeIndex>          free_nodes;
        HashMap<Entity, NodeIndex> entity_to_node;
    };

} // namespace lyra

#endif // LYRA_LYRA_SCENES_SCENE_TREE_H
