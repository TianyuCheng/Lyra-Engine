#pragma once

#ifndef LYRA_ENGINE_RUNTIME_SCENE_LAYER_H
#define LYRA_ENGINE_RUNTIME_SCENE_LAYER_H

#include <Lyra/Scene/World.h>
#include <Lyra/Scene/SceneTree.h>

// local import
#include <Lyra/Runtime/Application.h>

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
         * @brief Register the World and SceneTree to the toolboard.
         */
        void bind(Application& app);

        /**
         * @brief Main update loop for the scene.
         */
        void update(AppContext&);

    private:
        World     world;     ///< The ECS world instance.
        SceneTree hierarchy; ///< The scene graph hierarchy for the world.
    };

} // namespace lyra

#endif // LYRA_ENGINE_RUNTIME_SCENE_LAYER_H
