#include <thread>
#include <nfd.hpp>
#include <boxer/boxer.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/UISystem/UIDialog.h>

using namespace lyra;
using namespace lyra::ui;
using namespace lyra::ui::dialog;

struct FilterStorage
{
    Vector<String>            names;
    Vector<String>            specs;
    Vector<nfdu8filteritem_t> items;
};

static String clean_filter_spec(const String& spec)
{
    String result;
    result.reserve(spec.size());
    for (size_t i = 0; i < spec.size();) {
        while (i < spec.size() && (spec[i] == ' ' || spec[i] == '\t' || spec[i] == ';' || spec[i] == ',')) {
            ++i;
        }
        String token;
        while (i < spec.size() && spec[i] != ';' && spec[i] != ',' && spec[i] != ' ' && spec[i] != '\t') {
            token.push_back(spec[i]);
            ++i;
        }
        if (token.empty()) continue;
        if (token == "*.*" || token == "*") {
            if (!result.empty()) result.push_back(',');
            result.push_back('*');
            continue;
        }
        size_t start = 0;
        while (start < token.size() && (token[start] == '*' || token[start] == '.')) {
            ++start;
        }
        String ext = token.substr(start);
        if (!ext.empty()) {
            if (!result.empty()) result.push_back(',');
            result.append(ext);
        }
    }
    return result;
}

static FilterStorage prepare_filters(const lyra::Vector<lyra::ui::dialog::Filter>& filters)
{
    FilterStorage storage;
    storage.names.reserve(filters.size());
    storage.specs.reserve(filters.size());
    storage.items.reserve(filters.size());

    for (const auto& f : filters) {
        storage.names.push_back(f.name);
        storage.specs.push_back(clean_filter_spec(f.spec));
    }

    for (size_t i = 0; i < filters.size(); ++i) {
        storage.items.push_back({storage.names[i].c_str(), storage.specs[i].c_str()});
    }

    return storage;
}

static String path_to_utf8(const lyra::Path& path)
{
    if (path.empty()) return {};
    auto u8 = path.u8string();
    return String(reinterpret_cast<const char*>(u8.data()), u8.size());
}

static lyra::Path utf8_to_path(const char* utf8_str)
{
    if (!utf8_str || *utf8_str == '\0') return {};
    return std::filesystem::u8path(utf8_str);
}

auto lyra::ui::dialog::open_file(const Options& options) -> std::optional<Path>
{
    NFD::Guard nfd_guard;

    auto               filter_storage   = prepare_filters(options.filters);
    String             default_path_str = path_to_utf8(options.default_path);
    const nfdu8char_t* default_path     = default_path_str.empty() ? nullptr : default_path_str.c_str();

    NFD::UniquePath out_path;
    nfdresult_t     result = NFD::OpenDialog(
        out_path,
        filter_storage.items.empty() ? nullptr : filter_storage.items.data(),
        static_cast<nfdfiltersize_t>(filter_storage.items.size()),
        default_path);

    if (result == NFD_OKAY && out_path) {
        return utf8_to_path(out_path.get());
    }

    return std::nullopt;
}

auto lyra::ui::dialog::open_files(const Options& options) -> Vector<Path>
{
    Vector<Path> paths;
    NFD::Guard   nfd_guard;

    auto               filter_storage   = prepare_filters(options.filters);
    String             default_path_str = path_to_utf8(options.default_path);
    const nfdu8char_t* default_path     = default_path_str.empty() ? nullptr : default_path_str.c_str();

    NFD::UniquePathSet out_paths;
    nfdresult_t        result = NFD::OpenDialogMultiple(
        out_paths,
        filter_storage.items.empty() ? nullptr : filter_storage.items.data(),
        static_cast<nfdfiltersize_t>(filter_storage.items.size()),
        default_path);

    if (result == NFD_OKAY && out_paths) {
        nfdpathsetsize_t count = 0;
        if (NFD::PathSet::Count(out_paths, count) == NFD_OKAY) {
            paths.reserve(count);
            for (nfdpathsetsize_t i = 0; i < count; ++i) {
                NFD::UniquePathSetPath out_path;
                if (NFD::PathSet::GetPath(out_paths, i, out_path) == NFD_OKAY && out_path) {
                    paths.push_back(utf8_to_path(out_path.get()));
                }
            }
        }
    }

    return paths;
}

auto lyra::ui::dialog::save_file(const Options& options) -> std::optional<Path>
{
    NFD::Guard nfd_guard;

    auto               filter_storage   = prepare_filters(options.filters);
    String             default_path_str = path_to_utf8(options.default_path);
    const nfdu8char_t* default_path     = default_path_str.empty() ? nullptr : default_path_str.c_str();

    NFD::UniquePath out_path;
    nfdresult_t     result = NFD::SaveDialog(
        out_path,
        filter_storage.items.empty() ? nullptr : filter_storage.items.data(),
        static_cast<nfdfiltersize_t>(filter_storage.items.size()),
        default_path,
        nullptr);

    if (result == NFD_OKAY && out_path) {
        return utf8_to_path(out_path.get());
    }

    return std::nullopt;
}

auto lyra::ui::dialog::select_folder(const Options& options) -> std::optional<Path>
{
    NFD::Guard nfd_guard;

    String             default_path_str = path_to_utf8(options.default_path);
    const nfdu8char_t* default_path     = default_path_str.empty() ? nullptr : default_path_str.c_str();

    NFD::UniquePath out_path;
    nfdresult_t     result = NFD::PickFolder(out_path, default_path);

    if (result == NFD_OKAY && out_path) {
        return utf8_to_path(out_path.get());
    }

    return std::nullopt;
}

void lyra::ui::dialog::open_file_async(const Options& options, OnFileSelected on_selected)
{
    std::thread([options, cb = std::move(on_selected)]() mutable {
        auto res = open_file(options);
        if (cb) {
            cb(res);
        }
    }).detach();
}

void lyra::ui::dialog::save_file_async(const Options& options, OnFileSelected on_selected)
{
    std::thread([options, cb = std::move(on_selected)]() mutable {
        auto res = save_file(options);
        if (cb) {
            cb(res);
        }
    }).detach();
}

void lyra::ui::dialog::select_folder_async(const Options& options, OnFileSelected on_selected)
{
    std::thread([options, cb = std::move(on_selected)]() mutable {
        auto res = select_folder(options);
        if (cb) {
            cb(res);
        }
    }).detach();
}

bool lyra::ui::dialog::confirm(const String& title, const String& message)
{
    boxer::Selection sel = boxer::show(message.c_str(), title.c_str(), boxer::Style::Question, boxer::Buttons::YesNo);
    return (sel == boxer::Selection::Yes);
}

void lyra::ui::dialog::alert(const String& title, const String& message, StatusRole role)
{
    boxer::Style style = boxer::Style::Info;
    if (role == StatusRole::Warning)
        style = boxer::Style::Warning;
    else if (role == StatusRole::Error || role == StatusRole::Critical)
        style = boxer::Style::Error;

    boxer::show(message.c_str(), title.c_str(), style, boxer::Buttons::OK);
}
