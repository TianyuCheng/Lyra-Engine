#pragma once

#ifndef LYRA_LYRA_PLAYER_CAMERA_LAYER_H
#define LYRA_LYRA_PLAYER_CAMERA_LAYER_H

#include <Lyra/Scenes/Camera.h>
#include <Lyra/Scenes/World.h>

// local imports
#include <Lyra/Player/Application.h>

namespace lyra
{
    /**
     * @brief The CameraLayer struct manages the camera systems and projection calculations.
     */
    struct CameraLayer
    {
    public:
        /**
         * @brief Default constructor for CameraLayer.
         */
        explicit CameraLayer();

        /**
         * @brief Bind the camera update function to application events.
         */
        void bind(Application& app);

        /**
         * @brief Updates all cameras in the world, recalculating their projection matrices.
         * @param blackboard The application's blackboard containing the World.
         */
        void update(Blackboard& blackboard);

    private:
        /**
         * @brief Recalculates projection matrices for all perspective cameras.
         */
        void update_perspective(World& world);

        /**
         * @brief Recalculates projection matrices for all orthographic cameras.
         */
        void update_orthographic(World& world);
    };

} // namespace lyra

#endif // LYRA_LYRA_PLAYER_CAMERA_LAYER_H
