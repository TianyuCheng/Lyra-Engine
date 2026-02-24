#pragma once

#ifndef LYRA_LIBRARY_RUNTIME_CAMERA_LAYER_H
#define LYRA_LIBRARY_RUNTIME_CAMERA_LAYER_H

#include <Lyra/Scene/Camera.h>
#include <Lyra/Scene/World.h>

// local imports
#include "Application.h"

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

#endif // LYRA_LIBRARY_RUNTIME_CAMERA_LAYER_H
