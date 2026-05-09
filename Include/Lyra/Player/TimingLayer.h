#pragma once

#ifndef LYRA_LYRA_PLAYER_TIMING_LAYER_H
#define LYRA_LYRA_PLAYER_TIMING_LAYER_H

#include <chrono>

// local imports
#include <Lyra/Player/Application.h>

namespace lyra
{
    /**
     * @brief The Clock struct holds the current timing information.
     * This is intended to be stored in the Blackboard for global access.
     */
    struct Clock
    {
        float delta_time = 0.0f; ///< Elapsed time since the last frame (seconds).
        float total_time = 0.0f; ///< Total elapsed time since application start (seconds).
        float time_scale = 1.0f; ///< Scale factor for delta time.
        bool  paused     = false; ///< If true, delta_time will be zero.
    };

    /**
     * @brief TimingLayer handles the high-resolution timing of the application.
     */
    struct TimingLayer
    {
    public:
        /**
         * @brief Default constructor that initializes the high-resolution clock.
         */
        explicit TimingLayer();

        /**
         * @brief Register the Clock to the blackboard and bind its update event.
         */
        void bind(Application& app);

        /**
         * @brief Main update loop for calculating delta time and total time.
         */
        void update(Blackboard& blackboard);

        /**
         * @brief Pause the application's clock.
         */
        void pause() { clock.paused = true; }
        /**
         * @brief Resume the application's clock.
         */
        void resume() { clock.paused = false; }
        /**
         * @brief Set the time scale for the application.
         */
        void set_time_scale(float scale) { clock.time_scale = scale; }

    private:
        Clock clock; ///< The timing state of the application.
        std::chrono::high_resolution_clock::time_point start_time;      ///< Time when the application started.
        std::chrono::high_resolution_clock::time_point last_frame_time; ///< Time of the previous frame update.
    };

} // namespace lyra

#endif // LYRA_LYRA_PLAYER_TIMING_LAYER_H
