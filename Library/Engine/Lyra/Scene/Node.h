#pragma once

#ifndef LYRA_LIBRARY_SCENE_NODE_H
#define LYRA_LIBRARY_SCENE_NODE_H

#include <Lyra/Common/ECS.h>
#include <Lyra/Common/Collections.h>

namespace lyra
{

    struct SceneNode
    {
        Entity entity;

        // clang-format off
        SceneNode()              : entity(entt::null) {}
        SceneNode(Entity entity) : entity(entity) {}

        operator Entity()          { return entity; }
        operator Entity() const    { return entity; }
        // clang-format on
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
