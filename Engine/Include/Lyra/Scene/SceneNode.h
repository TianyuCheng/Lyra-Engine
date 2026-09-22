#pragma once

#ifndef LYRA_ENGINE_SCENE_SCENE_NODE_H
#define LYRA_ENGINE_SCENE_SCENE_NODE_H

#include <Lyra/Utilities/ECS.h>
#include <Lyra/Utilities/Macros.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Collections.h>

namespace lyra
{

    struct SceneNode
    {
        Entity entity;

        // clang-format off
        FORCE_INLINE SceneNode()              : entity(entt::null) {}
        FORCE_INLINE SceneNode(Entity entity) : entity(entity) {}

        FORCE_INLINE operator Entity()          { return entity; }
        FORCE_INLINE operator Entity() const    { return entity; }
        // clang-format on
    };

    struct NodeName
    {
        String name = "unamed";
    };

    struct Parent
    {
        SceneNode node;
    };

    struct Children
    {
        SmallVector<SceneNode, 4> nodes;
    };

} // namespace lyra

#endif // LYRA_ENGINE_SCENE_SCENE_NODE_H
