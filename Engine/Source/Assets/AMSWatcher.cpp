#include <Lyra/Assets/AMSWatcher.h>
#include <Lyra/Utilities/Logger.h>

using namespace lyra;

static bool is_ignored_path(const Path& path)
{
    auto filename = path.filename().string();
    if (filename.empty()) return true;

    // ignore hidden files and directories
    if (filename.front() == '.') return true;

    // ignore .import sidecar files to avoid feedback loops with cookers
    if (path.extension() == ".import") return true;

    // ignore common temporary and swap files
    auto ext = path.extension().string();
    if (ext == ".tmp" || ext == ".crswap" || ext == ".swp" || ext == ".lock") return true;
    if (filename.front() == '~' || filename.back() == '~') return true;

    return false;
}

struct AssetWatcherListener : efsw::FileWatchListener
{
    AssetWatcher* owner = nullptr;

    explicit AssetWatcherListener(AssetWatcher* owner) : owner(owner) {}

    void handleFileAction(efsw::WatchID, const std::string& dir, const std::string& filename,
                          efsw::Action action, std::string old_filename) override
    {
        if (owner) {
            owner->on_file_action(dir, filename, action, old_filename);
        }
    }
};

AssetWatcher::AssetWatcher(Path watch_root, float debounce_seconds)
    : watch_root(std::move(watch_root)), debounce_seconds(debounce_seconds)
{
}

AssetWatcher::~AssetWatcher()
{
    stop();
}

AssetWatcher::AssetWatcher(AssetWatcher&& other) noexcept
    : watch_root(std::move(other.watch_root))
    , debounce_seconds(other.debounce_seconds)
    , running(other.running.load())
    , paused(other.paused.load())
    , stop_requested(other.stop_requested.load())
    , callback(std::move(other.callback))
    , pending_map(std::move(other.pending_map))
    , debounce_thread(std::move(other.debounce_thread))
    , file_watcher(std::move(other.file_watcher))
    , listener(std::move(other.listener))
    , watch_id(other.watch_id)
{
    other.running.store(false);
    other.paused.store(false);
    other.stop_requested.store(false);
    other.watch_id = 0;
}

AssetWatcher& AssetWatcher::operator=(AssetWatcher&& other) noexcept
{
    if (this != &other) {
        stop();
        watch_root         = std::move(other.watch_root);
        debounce_seconds   = other.debounce_seconds;
        running.store(other.running.load());
        paused.store(other.paused.load());
        stop_requested.store(other.stop_requested.load());
        callback           = std::move(other.callback);
        pending_map        = std::move(other.pending_map);
        debounce_thread    = std::move(other.debounce_thread);
        file_watcher       = std::move(other.file_watcher);
        listener           = std::move(other.listener);
        watch_id           = other.watch_id;

        other.running.store(false);
        other.paused.store(false);
        other.stop_requested.store(false);
        other.watch_id = 0;
    }
    return *this;
}

bool AssetWatcher::start(WatchCallback cb)
{
    if (running.load()) return true;

    callback = std::move(cb);

    if (!fs::exists(watch_root)) {
        spdlog::warn("AssetWatcher: root directory does not exist: {}", watch_root.string());
        return false;
    }

    file_watcher = std::make_unique<efsw::FileWatcher>();
    listener     = std::make_unique<AssetWatcherListener>(this);

    // add recursive watch
    watch_id = file_watcher->addWatch(watch_root.string(), listener.get(), true);
    if (watch_id <= 0) {
        spdlog::error("AssetWatcher: efsw failed to add watch on {}", watch_root.string());
        file_watcher.reset();
        listener.reset();
        return false;
    }

    file_watcher->watch();

    stop_requested.store(false);
    running.store(true);
    debounce_thread = std::thread(&AssetWatcher::run_debounce_worker, this);

    spdlog::info("AssetWatcher: watching started via efsw on {}", watch_root.string());
    return true;
}

void AssetWatcher::stop()
{
    if (!running.exchange(false)) return;

    stop_requested.store(true);
    pending_cv.notify_all();

    if (debounce_thread.joinable()) {
        debounce_thread.join();
    }

    if (file_watcher) {
        if (watch_id > 0) {
            file_watcher->removeWatch(watch_id);
            watch_id = 0;
        }
        file_watcher.reset();
    }

    listener.reset();

    {
        std::lock_guard lock(pending_mutex);
        pending_map.clear();
    }
}

void AssetWatcher::pause()
{
    paused.store(true);
}

void AssetWatcher::resume()
{
    paused.store(false);
}

bool AssetWatcher::is_running() const
{
    return running.load();
}

bool AssetWatcher::is_paused() const
{
    return paused.load();
}

const Path& AssetWatcher::get_watch_root() const
{
    return watch_root;
}

void AssetWatcher::on_file_action(const std::string& dir, const std::string& filename,
                                  efsw::Action action, const std::string& old_filename)
{
    Path full_path = Path(dir) / filename;
    std::error_code ec;
    Path rel_path = fs::relative(full_path, watch_root, ec);
    if (ec || is_ignored_path(rel_path)) return;

    AssetWatchAction watch_action = AssetWatchAction::Modified;
    bool valid = true;

    switch (action) {
        case efsw::Actions::Add:
            watch_action = AssetWatchAction::Added;
            break;
        case efsw::Actions::Delete:
            watch_action = AssetWatchAction::Removed;
            break;
        case efsw::Actions::Modified:
            watch_action = AssetWatchAction::Modified;
            break;
        case efsw::Actions::Moved:
            watch_action = AssetWatchAction::Renamed;
            break;
        default:
            valid = false;
            break;
    }

    if (!valid) return;

    auto now          = std::chrono::steady_clock::now();
    auto sys_now      = std::chrono::system_clock::now();
    auto debounce_dur = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<float>(debounce_seconds));

    std::lock_guard lock(pending_mutex);
    String key = rel_path.generic_string();

    auto it = pending_map.find(key);
    if (it != pending_map.end()) {
        if (it->second.event.action == AssetWatchAction::Added &&
            watch_action == AssetWatchAction::Modified) {
            watch_action = AssetWatchAction::Added;
        }
        it->second.event.action    = watch_action;
        it->second.deadline        = now + debounce_dur;
        it->second.event.timestamp = sys_now;
    } else {
        AssetWatchEvent evt;
        evt.action    = watch_action;
        evt.path      = rel_path;
        evt.timestamp = sys_now;
        if (watch_action == AssetWatchAction::Renamed && !old_filename.empty()) {
            evt.old_path = fs::relative(Path(dir) / old_filename, watch_root, ec);
        }
        pending_map.emplace(key, AssetWatchPendingEvent{evt, now + debounce_dur});
    }

    pending_cv.notify_one();
}

void AssetWatcher::run_debounce_worker()
{
    while (!stop_requested.load()) {
        std::unique_lock lock(pending_mutex);
        pending_cv.wait_for(lock, std::chrono::milliseconds(50), [&]() {
            return stop_requested.load();
        });

        if (stop_requested.load()) break;

        auto now = std::chrono::steady_clock::now();
        Vector<AssetWatchEvent> ready_events;

        for (auto it = pending_map.begin(); it != pending_map.end();) {
            if (now >= it->second.deadline) {
                ready_events.push_back(std::move(it->second.event));
                pending_map.erase(it++);
            } else {
                ++it;
            }
        }

        lock.unlock();

        if (!ready_events.empty() && !paused.load() && callback) {
            callback(ready_events);
        }
    }
}
