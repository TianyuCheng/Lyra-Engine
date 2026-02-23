#pragma once

#ifndef LYRA_LIBRARY_SCENE_NODE_H
#define LYRA_LIBRARY_SCENE_NODE_H

#include <Lyra/Common/ECS.h>
#include <Lyra/Common/Macros.h>
#include <Lyra/Common/String.h>
#include <Lyra/Common/Collections.h>

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

#endif // LYRA_LIBRARY_SCENE_NODE_H
