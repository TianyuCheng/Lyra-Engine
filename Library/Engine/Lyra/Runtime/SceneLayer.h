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
    /**
     * @brief The SceneLayer struct manages the ECS world and its corresponding scene tree.
     */
    struct SceneLayer
    {
    public:
        /**
         * @brief Default constructor for SceneLayer.
         */
        explicit SceneLayer();

        /**
         * @brief Register the World and SceneTree to the blackboard.
         */
        void bind(Application& app);

        /**
         * @brief Main update loop for the scene.
         */
        void update(Blackboard&);

    private:
        World     world;     ///< The ECS world instance.
        SceneTree hierarchy; ///< The scene graph hierarchy for the world.
    };

} // namespace lyra

#endif // LYRA_LIBRARY_RUNTIME_SCENE_LAYER_H
