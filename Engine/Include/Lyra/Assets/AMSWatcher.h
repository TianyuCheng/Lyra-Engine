#pragma once

#ifndef LYRA_LYRA_ASSETS_AMSWATCHER_H
#define LYRA_LYRA_ASSETS_AMSWATCHER_H

#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>
#include <condition_variable>
#include <efsw/efsw.hpp>

#include <Lyra/Common/Path.h>
#include <Lyra/Common/String.h>
#include <Lyra/Common/Pointer.h>
#include <Lyra/Common/Function.h>
#include <Lyra/Common/Collections.h>
#include <Lyra/Assets/AMSEnums.h>

namespace lyra
{
    /**
     * @brief Represents a filesystem event detected by the AssetWatcher.
     */
    struct AssetWatchEvent
    {
        AssetWatchAction                      action;
        Path                                  path;     ///< Path relative to the watched root directory.
        Path                                  old_path; ///< Previous path if action == AssetWatchAction::Renamed.
        std::chrono::system_clock::time_point timestamp;
    };

    /**
     * @brief Internal tracking for debouncing filesystem events.
     */
    struct AssetWatchPendingEvent
    {
        AssetWatchEvent                       event;
        std::chrono::steady_clock::time_point deadline;
    };

    /**
     * @brief Cross-platform directory watcher based on efsw with event debouncing and filtering.
     */
    struct AssetWatcher
    {
    public:
        using WatchCallback = Function<void(const Vector<AssetWatchEvent>&)>;

        explicit AssetWatcher(Path watch_root, float debounce_seconds = 0.25f);
        ~AssetWatcher();

        AssetWatcher(const AssetWatcher&)            = delete;
        AssetWatcher& operator=(const AssetWatcher&) = delete;
        AssetWatcher(AssetWatcher&&) noexcept;
        AssetWatcher& operator=(AssetWatcher&&) noexcept;

        /**
         * @brief Start monitoring the directory in a background thread.
         */
        bool start(WatchCallback callback);

        /**
         * @brief Stop monitoring and wait for background threads to exit.
         */
        void stop();

        /**
         * @brief Temporarily pause event dispatching.
         */
        void pause();

        /**
         * @brief Resume event dispatching.
         */
        void resume();

        /**
         * @brief Check whether the watcher is actively running.
         */
        bool is_running() const;

        /**
         * @brief Check whether the watcher is currently paused.
         */
        bool is_paused() const;

        /**
         * @brief Get the root directory being watched.
         */
        const Path& get_watch_root() const;

        /**
         * @brief Internal callback dispatched by efsw listener.
         */
        void on_file_action(const std::string& dir, const std::string& filename,
            efsw::Action action, const std::string& old_filename);

    private:
        void run_debounce_worker();

    private:
        Path              watch_root;
        float             debounce_seconds;
        std::atomic<bool> running{false};
        std::atomic<bool> paused{false};
        std::atomic<bool> stop_requested{false};
        WatchCallback     callback;

        std::mutex                              pending_mutex;
        std::condition_variable                 pending_cv;
        HashMap<String, AssetWatchPendingEvent> pending_map;
        std::thread                             debounce_thread;

        Own<efsw::FileWatcher>       file_watcher;
        Own<efsw::FileWatchListener> listener;
        efsw::WatchID                watch_id{0};
    };

} // namespace lyra

#endif // LYRA_LYRA_ASSETS_AMSWATCHER_H
