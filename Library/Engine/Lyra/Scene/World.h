#pragma once

#ifndef LYRA_LIBRARY_SCENE_WORLD_H
#define LYRA_LIBRARY_SCENE_WORLD_H

#include <Lyra/Common/ECS.h>

// local imports
#include "SceneNode.h"
#include "Transform.h"

namespace lyra
{

    /**
     * @brief The World struct manages the ECS registry and scene graph operations.
     */
    struct World
    {
        // underlying ECS registry.
        Registry registry;

        /**
         * @brief Access the underlying registry.
         * @return Reference to the registry.
         */
        auto& operator->() { return registry; }

        /**
         * @brief Access the underlying registry (const).
         * @return Const reference to the registry.
         */
        const auto& operator->() const { return registry; }

        /**
         * @brief Create a view of entities having the specified components.
         * @tparam T Types of components to include in the view.
         * @return An EnTT view.
         */
        template <typename... T>
        auto view() { return registry.view<T...>(); }

        /**
         * @brief Create a view for a specific node.
         * @tparam T Types of components.
         * @param node The scene node.
         * @return An EnTT view.
         */
        template <typename... T>
        auto view(const SceneNode node) { return registry.view<T...>(node.entity); }

        /**
         * @brief Check if a node has any of the specified components.
         * @tparam T Types of components.
         * @param node The scene node.
         * @return True if the node has any of the components, false otherwise.
         */
        template <typename... T>
        bool any_of(const SceneNode node) const
        {
            return registry.any_of<T...>(node.entity);
        }

        /**
         * @brief Check if a node has all of the specified components.
         * @tparam T Types of components.
         * @param node The scene node.
         * @return True if the node has all of the components, false otherwise.
         */
        template <typename... T>
        bool all_of(const SceneNode node) const
        {
            return registry.all_of<T...>(node.entity);
        }

        /**
         * @brief Add or replace a component on a scene node.
         * @tparam T Type of the component.
         * @tparam Args Argument types for component construction.
         * @param node The scene node.
         * @param args Arguments for component construction.
         */
        template <typename T, typename... Args>
        void add_component(const SceneNode node, Args... args)
        {
            registry.emplace_or_replace<T>(node, std::forward<Args>(args)...);
        }

        template <typename... T>
        decltype(auto) get_component(const SceneNode node)
        {
            return registry.get<T...>(node);
        }

        template <typename... T>
        decltype(auto) get_component(const SceneNode node) const
        {
            return registry.get<T...>(node);
        }

        /**
         * @brief Create a new entity with default TransformLocal and TransformWorld components.
         * @return The created Entity.
         */
        auto create() -> Entity
        {
            // every entity in the scene must have transforms attached
            auto node = registry.create();
            registry.emplace<TransformLocal>(node);
            registry.emplace<TransformWorld>(node);
            return node;
        }

        /**
         * @brief Add a child node to a parent node.
         * @param node The parent node.
         * @param child The child node to add.
         */
        void add_child(const SceneNode node, const SceneNode child)
        {
            // connect parent node
            registry.emplace_or_replace<Parent>(child, node);

            // connect child nodes
            registry.get_or_emplace<Children>(node)
                .nodes.push_back(child);
        }

        /**
         * @brief Remove a child node from its parent.
         * @param node The parent node.
         * @param child The child node to remove.
         */
        void del_child(const SceneNode node, const SceneNode child)
        {
            // delete parent node
            registry.remove<Parent>(child);

            // delete children nodes
            registry.get_or_emplace<Children>(node)
                .nodes.remove(child);
        }

        /**
         * @brief Set the parent of a node.
         * @param node The node.
         * @param parent The parent node to set.
         */
        void set_parent(const SceneNode node, const SceneNode parent)
        {
            add_child(parent, node);
        }

        /**
         * @brief Change the parent of a node, removing it from its old parent if it exists.
         * @param node The node to reparent.
         * @param parent The new parent node.
         */
        void reparent(const SceneNode node, const SceneNode parent)
        {
            // delete from old parent
            auto old_parent = registry.try_get<Parent>(node);
            if (old_parent) {
                del_child(old_parent->node, node);
            }

            // connect new parent node
            add_child(parent, node);
        }

        /**
         * @brief Multiply the local scale of a node.
         * @param node The scene node.
         * @param scale The scale factor.
         */
        void scale(const SceneNode node, const Vector3& scale)
        {
            auto& transform = registry.get_or_emplace<TransformLocal>(node);
            transform.scale *= scale;
            transform.flags |= TransformFlag::LOCAL_DIRTY;
        }

        /**
         * @brief Translate the local position of a node.
         * @param node The scene node.
         * @param translation The translation vector.
         */
        void translate(const SceneNode node, const Vector3& translation)
        {
            auto& transform = registry.get_or_emplace<TransformLocal>(node);
            transform.position += translation;
            transform.flags |= TransformFlag::LOCAL_DIRTY;
        }

        /**
         * @brief Rotate the local rotation of a node by a quaternion.
         * @param node The scene node.
         * @param rotation The rotation quaternion.
         */
        void rotate(const SceneNode node, const Quaternion& rotation)
        {
            auto& transform = registry.get_or_emplace<TransformLocal>(node);
            transform.rotation *= rotation;
            transform.flags |= TransformFlag::LOCAL_DIRTY;
        }

        /**
         * @brief Rotate the local rotation of a node using axis-angle.
         * @param node The scene node.
         * @param axis The rotation axis.
         * @param angle The rotation angle in degrees.
         */
        void rotate(const SceneNode node, const Vector3& axis, float angle)
        {
            rotate(node, glm::angleAxis(glm::radians(angle), axis));
        }
    };

} // namespace lyra

#endif // LYRA_LIBRARY_SCENE_WORLD_H
