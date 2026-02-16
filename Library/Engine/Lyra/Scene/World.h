#pragma once

#ifndef LYRA_LIBRARY_SCENE_WORLD_H
#define LYRA_LIBRARY_SCENE_WORLD_H

#include <Lyra/Common/ECS.h>

// local imports
#include "Node.h"
#include "Transform.h"

namespace lyra
{

    struct World
    {
        Registry registry;

        // clang-format off
        auto operator->() -> Registry& { return registry; }
        auto operator->() const -> const Registry& { return registry; }
        // clang-format on

        template <typename... T>
        auto view()
        {
            return registry.view<T...>();
        }

        template <typename... T>
        auto view(SceneNode node)
        {
            return registry.view<T...>(node.entity);
        }

        template <typename T, typename... Args>
        void add_component(SceneNode node, Args... args)
        {
            registry.emplace_or_replace<T>(node, std::forward<Args...>(args...));
        }

        auto create() -> Entity
        {
            // every entity in the scene must have transforms attached
            auto node = registry.create();
            registry.emplace<TransformLocal>(node);
            registry.emplace<TransformWorld>(node);
            return node;
        }

        void add_child(SceneNode node, SceneNode child)
        {
            // connect parent node
            registry.emplace_or_replace<Parent>(child, node);

            // connect child nodes
            registry.get_or_emplace<Children>(node)
                .nodes.push_back(child);
        }

        void del_child(SceneNode node, SceneNode child)
        {
            // delete parent node
            registry.remove<Parent>(child);

            // delete children nodes
            registry.get_or_emplace<Children>(node)
                .nodes.remove(child);
        }

        void set_parent(SceneNode node, SceneNode parent)
        {
            add_child(parent, node);
        }

        void reparent(SceneNode node, SceneNode parent)
        {
            // delete from old parent
            auto old_parent = registry.try_get<Parent>(node);
            if (old_parent) {
                del_child(old_parent->node, node);
            }

            // connect new parent node
            add_child(parent, node);
        }

        void scale(SceneNode node, const Vector3& scale)
        {
            auto& transform = registry.get_or_emplace<TransformLocal>(node);
            transform.scale *= scale;
            transform.flags |= TransformFlag::LOCAL_DIRTY;
        }

        void translate(SceneNode node, const Vector3& translation)
        {
            auto& transform = registry.get_or_emplace<TransformLocal>(node);
            transform.position += translation;
            transform.flags |= TransformFlag::LOCAL_DIRTY;
        }

        void rotate(SceneNode node, const Quaternion& rotation)
        {
            auto& transform = registry.get_or_emplace<TransformLocal>(node);
            transform.rotation *= rotation;
            transform.flags |= TransformFlag::LOCAL_DIRTY;
        }

        void rotate(SceneNode node, const Vector3& axis, float angle)
        {
            rotate(node, glm::angleAxis(glm::radians(angle), axis));
        }
    };

} // namespace lyra

#endif // LYRA_LIBRARY_SCENE_WORLD_H
