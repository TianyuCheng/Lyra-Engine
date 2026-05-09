#pragma once

#ifndef LYRA_LYRA_PLAYER_SCENE_LAYER_H
#define LYRA_LYRA_PLAYER_SCENE_LAYER_H

#include <Lyra/Common/GUI.h>
#include <Lyra/Scenes/World.h>
#include <Lyra/Scenes/SceneTree.h>

// local import
#include <Lyra/Player/Application.h>

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

#endif // LYRA_LYRA_PLAYER_SCENE_LAYER_H
