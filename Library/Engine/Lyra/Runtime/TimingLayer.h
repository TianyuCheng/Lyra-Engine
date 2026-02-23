#pragma once

#ifndef LYRA_LIBRARY_RUNTIME_TIMING_LAYER_H
#define LYRA_LIBRARY_RUNTIME_TIMING_LAYER_H

#include <chrono>

// local imports
#include "Application.h"

namespace lyra
{
    /**
     * @brief The Clock struct holds the current timing information.
     * This is intended to be stored in the Blackboard for global access.
     */
    struct Clock
    {
        float delta_time = 0.0f;
        float total_time = 0.0f;
        float time_scale = 1.0f;
        bool  paused     = false;
    };

    /**
     * @brief TimingLayer handles the high-resolution timing of the application.
     */
    struct TimingLayer
    {
    public:
        explicit TimingLayer();

        void bind(Application& app);

        void update(Blackboard& blackboard);

        void pause() { clock.paused = true; }
        void resume() { clock.paused = false; }
        void set_time_scale(float scale) { clock.time_scale = scale; }

    private:
        Clock clock;
        std::chrono::high_resolution_clock::time_point start_time;
        std::chrono::high_resolution_clock::time_point last_frame_time;
    };

} // namespace lyra

#endif // LYRA_LIBRARY_RUNTIME_TIMING_LAYER_H
