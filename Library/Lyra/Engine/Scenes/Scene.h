#pragma once

#ifndef LYRA_LIBRARY_SCENES_SCENE_H
#define LYRA_LIBRARY_SCENES_SCENE_H

#include <Lyra/Vendor/ECS.h>
#include <Lyra/Scenes/Transform.h>

namespace lyra
{

    struct SceneNode
    {
        Entity entity;

        SceneNode() : entity(entt::null)
        {
            // initialize with invalid entity
        }

        SceneNode(Entity entity) : entity(entity)
        {
            // initialize with valid entity
        }

        operator Entity()
        {
            return entity;
        }

        operator Entity() const
        {
            return entity;
        }
    };

    struct ParentNode
    {
        SceneNode parent;
    };

    struct Universe
    {
        Registry registry;

        auto create() -> Entity
        {
            auto node = registry.create();

            // every entity in the scene must have transforms attached
            registry.emplace_or_replace<Transform>(node, Transform{});
            registry.emplace_or_replace<WorldTransform>(node, WorldTransform{});
            return node;
        }

        auto operator->() -> Registry&
        {
            return registry;
        }

        auto operator->() const -> const Registry&
        {
            return registry;
        }

        void set_parent(SceneNode node, SceneNode parent)
        {
            registry.emplace_or_replace<ParentNode>(node, parent);
        }

        void set_transform(SceneNode node, const Transform& transform)
        {
            registry.emplace_or_replace<Transform>(node, transform);
        }

        void add_child(SceneNode node, SceneNode child)
        {
            set_parent(child, node);
        }

        template <typename T, typename... Args>
        void add_component(SceneNode node, Args... args)
        {
            registry.emplace<T>(node, std::forward(args...));
        }

        template <typename... T>
        auto view()
        {
            return registry.view<T...>();
        }

        template <typename... T>
        auto view(SceneNode node)
        {
            return registry.view<T...>(node);
        }
    };

} // namespace lyra

#endif // LYRA_LIBRARY_SCENES_SCENE_H
