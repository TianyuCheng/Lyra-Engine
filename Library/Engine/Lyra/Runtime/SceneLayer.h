#pragma once

#ifndef LYRA_LIBRARY_RUNTIME_SCENE_LAYER_H
#define LYRA_LIBRARY_RUNTIME_SCENE_LAYER_H

#include <Lyra/Common/GUI.h>
#include <Lyra/Scene/World.h>
#include <Lyra/Scene/SceneTree.h>

// local import
#include "Application.h"

namespace lyra
{
    struct SceneLayer
    {
    public:
        explicit SceneLayer();

        void bind(Application& app);

        void update(Blackboard&);

    private:
        World     world;
        SceneTree hierarchy;
    };

} // namespace lyra

#endif // LYRA_LIBRARY_RUNTIME_SCENE_LAYER_H
