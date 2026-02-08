#pragma once

#ifndef LYRA_LIBRARY_ENGINE_SCENES_UNIVERSE_H
#define LYRA_LIBRARY_ENGINE_SCENES_UNIVERSE_H

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

        auto create() -> Entity
        {
            // every entity in the scene must have transforms attached
            auto node = registry.create();
            registry.emplace<Transform>(node, Transform{});
            registry.emplace<WorldTransform>(node, WorldTransform{});
            return node;
        }

        void set_parent(SceneNode node, SceneNode parent)
        {
            registry.emplace_or_replace<SceneNodeParent>(node, parent);
        }

        void set_transform(SceneNode node, const Transform& transform)
        {
            registry.emplace_or_replace<Transform>(node, transform);
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

#endif // LYRA_LIBRARY_ENGINE_SCENES_UNIVERSE_H
