#pragma once

#ifndef LYRA_LIBRARY_ENGINE_SCENES_SCENE_NODE_H
#define LYRA_LIBRARY_ENGINE_SCENES_SCENE_NODE_H

#include <Lyra/Common/ECS.h>

namespace lyra
{

    enum struct SceneNodeType
    {
        Simple,
        Parent,
    };

    template <SceneNodeType NODE_TYPE>
    struct SceneNodeTemplate
    {
        Entity entity;

        SceneNodeTemplate() : entity(entt::null) {}
        SceneNodeTemplate(Entity entity) : entity(entity) {}
        operator Entity() { return entity; }
        operator Entity() const { return entity; }
    };

    using SceneNode       = SceneNodeTemplate<SceneNodeType::Simple>;
    using SceneNodeParent = SceneNodeTemplate<SceneNodeType::Parent>;

} // namespace lyra

#endif // LYRA_LIBRARY_ENGINE_SCENES_SCENE_NODE_H
