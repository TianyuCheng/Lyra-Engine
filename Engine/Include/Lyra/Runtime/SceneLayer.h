#pragma once

#ifndef LYRA_ENGINE_RUNTIME_SCENE_LAYER_H
#define LYRA_ENGINE_RUNTIME_SCENE_LAYER_H

#include <Lyra/Scene/World.h>
#include <Lyra/Scene/SceneTree.h>
#include <Lyra/Scene/SceneManager.h>

// local import
#include <Lyra/Runtime/Application.h>

namespace lyra
{
    /**
     * @brief The SceneLayer struct manages the ECS world, scene tree, and scene manager.
     */
    struct SceneLayer
    {
    public:
        /**
         * @brief Default constructor for SceneLayer.
         */
        explicit SceneLayer();

        /**
         * @brief Register the World, SceneTree, and SceneManager to the toolboard.
         */
        void bind(Application& app);

        /**
         * @brief Main update loop for the scene.
         */
        void update(AppContext&);

        /**
         * @brief Access the SceneManager instance.
         */
        FORCE_INLINE SceneManager& get_manager() { return *scene_manager; }
        FORCE_INLINE const SceneManager& get_manager() const { return *scene_manager; }

    private:
        World             world;         ///< The ECS world instance.
        SceneTree         hierarchy;     ///< The scene graph hierarchy for the world.
        Own<SceneManager> scene_manager; ///< The scene manager instance.
    };

} // namespace lyra

#endif // LYRA_ENGINE_RUNTIME_SCENE_LAYER_H
