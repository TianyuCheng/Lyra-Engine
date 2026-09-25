#pragma once

#ifndef LYRA_ENGINE_RUNTIME_APP_ENUMS_H
#define LYRA_ENGINE_RUNTIME_APP_ENUMS_H

#include <Lyra/Utilities/Stdint.h>

namespace lyra
{
    /**
     * @brief Application lifecycle events.
     */
    enum struct AppEvent : uint
    {
        INIT,    ///< Called during application initialization.
        DESTROY, ///< Called during application destruction.
        RESIZE,  ///< Called when the application window is resized.

        UPDATE,       ///< Main update logic.
        UPDATE_PRE,   ///< Logic executed before the main update.
        UPDATE_POST,  ///< Logic executed after the main update.
        UPDATE_FIXED, ///< Fixed-rate update logic.

        UI,      ///< UI rendering logic.
        UI_PRE,  ///< Logic executed before UI rendering.
        UI_POST, ///< Logic executed after UI rendering.

        RENDER,      ///< Main rendering logic.
        RENDER_PRE,  ///< Logic executed before rendering.
        RENDER_POST, ///< Logic executed after rendering.
    };

    /**
     * @brief Application-wide simulation lifecycle state (Editor vs Game).
     */
    enum struct SimulationState : uint
    {
        EDIT,   ///< Edit mode: systems/physics pause or only run preview/editor systems
        PLAY,   ///< Play mode: full active simulation
        PAUSED  ///< Simulation paused: systems do not advance
    };

} // namespace lyra

#endif // LYRA_ENGINE_RUNTIME_APP_ENUMS_H
