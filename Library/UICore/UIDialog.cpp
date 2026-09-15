#include <future>
#include <boxer/boxer.h>
#include <Lyra/UICore/UIDialog.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shobjidl.h>
#include <wrl/client.h>

namespace
{
    using Microsoft::WRL::ComPtr;

    struct ScopedCoInitialize
    {
        HRESULT hr;
        ScopedCoInitialize() : hr(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)) {}
        ~ScopedCoInitialize()
        {
            if (SUCCEEDED(hr)) {
                CoUninitialize();
            }
        }
    };

    std::wstring to_wstring(const std::string& str)
    {
        if (str.empty()) return std::wstring();
        int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), nullptr, 0);
        std::wstring wstr(size, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), &wstr[0], size);
        return wstr;
    }

    std::string to_string(const std::wstring& wstr)
    {
        if (wstr.empty()) return std::string();
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
        std::string str(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), &str[0], size, nullptr, nullptr);
        return str;
    }
}
#endif

using namespace lyra;
using namespace lyra::ui;
using namespace lyra::ui::dialog;

auto lyra::ui::dialog::open_file(const Options& options) -> std::optional<Path>
{
#if defined(_WIN32)
    ScopedCoInitialize co_init;
    ComPtr<IFileOpenDialog> dialog;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dialog));
    if (FAILED(hr)) return std::nullopt;

    if (!options.title.empty()) {
        std::wstring wtitle = to_wstring(options.title);
        dialog->SetTitle(wtitle.c_str());
    }

    if (!options.default_path.empty()) {
        ComPtr<IShellItem> folder;
        std::wstring wpath = to_wstring(options.default_path.string());
        if (SUCCEEDED(SHCreateItemFromParsingName(wpath.c_str(), nullptr, IID_PPV_ARGS(&folder)))) {
            dialog->SetFolder(folder.Get());
        }
    }

    std::vector<std::wstring> name_storage;
    std::vector<std::wstring> spec_storage;
    std::vector<COMDLG_FILTERSPEC> filter_specs;

    for (const auto& f : options.filters) {
        name_storage.push_back(to_wstring(f.name));
        spec_storage.push_back(to_wstring(f.spec));
    }
    for (size_t i = 0; i < options.filters.size(); ++i) {
        filter_specs.push_back({name_storage[i].c_str(), spec_storage[i].c_str()});
    }

    if (!filter_specs.empty()) {
        dialog->SetFileTypes(static_cast<UINT>(filter_specs.size()), filter_specs.data());
    }

    hr = dialog->Show(nullptr);
    if (FAILED(hr)) return std::nullopt;

    ComPtr<IShellItem> result;
    hr = dialog->GetResult(&result);
    if (FAILED(hr)) return std::nullopt;

    PWSTR file_path = nullptr;
    hr = result->GetDisplayName(SIGDN_FILESYSPATH, &file_path);
    if (FAILED(hr) || !file_path) return std::nullopt;

    Path selected_path(to_string(file_path));
    CoTaskMemFree(file_path);
    return selected_path;
#else
    return std::nullopt;
#endif
}

auto lyra::ui::dialog::open_files(const Options& options) -> Vector<Path>
{
    Vector<Path> paths;
#if defined(_WIN32)
    ScopedCoInitialize co_init;
    ComPtr<IFileOpenDialog> dialog;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dialog));
    if (FAILED(hr)) return paths;

    FILEOPENDIALOGOPTIONS fos = 0;
    dialog->GetOptions(&fos);
    dialog->SetOptions(fos | FOS_ALLOWMULTISELECT);

    if (!options.title.empty()) {
        std::wstring wtitle = to_wstring(options.title);
        dialog->SetTitle(wtitle.c_str());
    }

    if (!options.default_path.empty()) {
        ComPtr<IShellItem> folder;
        std::wstring wpath = to_wstring(options.default_path.string());
        if (SUCCEEDED(SHCreateItemFromParsingName(wpath.c_str(), nullptr, IID_PPV_ARGS(&folder)))) {
            dialog->SetFolder(folder.Get());
        }
    }

    std::vector<std::wstring> name_storage;
    std::vector<std::wstring> spec_storage;
    std::vector<COMDLG_FILTERSPEC> filter_specs;
    for (const auto& f : options.filters) {
        name_storage.push_back(to_wstring(f.name));
        spec_storage.push_back(to_wstring(f.spec));
    }
    for (size_t i = 0; i < options.filters.size(); ++i) {
        filter_specs.push_back({name_storage[i].c_str(), spec_storage[i].c_str()});
    }
    if (!filter_specs.empty()) {
        dialog->SetFileTypes(static_cast<UINT>(filter_specs.size()), filter_specs.data());
    }

    hr = dialog->Show(nullptr);
    if (FAILED(hr)) return paths;

    ComPtr<IShellItemArray> items;
    hr = dialog->GetResults(&items);
    if (FAILED(hr)) return paths;

    DWORD count = 0;
    items->GetCount(&count);
    for (DWORD i = 0; i < count; ++i) {
        ComPtr<IShellItem> item;
        if (SUCCEEDED(items->GetItemAt(i, &item))) {
            PWSTR file_path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &file_path)) && file_path) {
                paths.push_back(Path(to_string(file_path)));
                CoTaskMemFree(file_path);
            }
        }
    }
#endif
    return paths;
}

auto lyra::ui::dialog::save_file(const Options& options) -> std::optional<Path>
{
#if defined(_WIN32)
    ScopedCoInitialize co_init;
    ComPtr<IFileSaveDialog> dialog;
    HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dialog));
    if (FAILED(hr)) return std::nullopt;

    if (!options.title.empty()) {
        std::wstring wtitle = to_wstring(options.title);
        dialog->SetTitle(wtitle.c_str());
    }

    if (!options.default_path.empty()) {
        ComPtr<IShellItem> folder;
        std::wstring wpath = to_wstring(options.default_path.string());
        if (SUCCEEDED(SHCreateItemFromParsingName(wpath.c_str(), nullptr, IID_PPV_ARGS(&folder)))) {
            dialog->SetFolder(folder.Get());
        }
    }

    std::vector<std::wstring> name_storage;
    std::vector<std::wstring> spec_storage;
    std::vector<COMDLG_FILTERSPEC> filter_specs;
    for (const auto& f : options.filters) {
        name_storage.push_back(to_wstring(f.name));
        spec_storage.push_back(to_wstring(f.spec));
    }
    for (size_t i = 0; i < options.filters.size(); ++i) {
        filter_specs.push_back({name_storage[i].c_str(), spec_storage[i].c_str()});
    }
    if (!filter_specs.empty()) {
        dialog->SetFileTypes(static_cast<UINT>(filter_specs.size()), filter_specs.data());
    }

    hr = dialog->Show(nullptr);
    if (FAILED(hr)) return std::nullopt;

    ComPtr<IShellItem> result;
    hr = dialog->GetResult(&result);
    if (FAILED(hr)) return std::nullopt;

    PWSTR file_path = nullptr;
    hr = result->GetDisplayName(SIGDN_FILESYSPATH, &file_path);
    if (FAILED(hr) || !file_path) return std::nullopt;

    Path selected_path(to_string(file_path));
    CoTaskMemFree(file_path);
    return selected_path;
#else
    return std::nullopt;
#endif
}

auto lyra::ui::dialog::select_folder(const Options& options) -> std::optional<Path>
{
#if defined(_WIN32)
    ScopedCoInitialize co_init;
    ComPtr<IFileOpenDialog> dialog;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dialog));
    if (FAILED(hr)) return std::nullopt;

    FILEOPENDIALOGOPTIONS fos = 0;
    dialog->GetOptions(&fos);
    dialog->SetOptions(fos | FOS_PICKFOLDERS);

    if (!options.title.empty()) {
        std::wstring wtitle = to_wstring(options.title);
        dialog->SetTitle(wtitle.c_str());
    }

    if (!options.default_path.empty()) {
        ComPtr<IShellItem> folder;
        std::wstring wpath = to_wstring(options.default_path.string());
        if (SUCCEEDED(SHCreateItemFromParsingName(wpath.c_str(), nullptr, IID_PPV_ARGS(&folder)))) {
            dialog->SetFolder(folder.Get());
        }
    }

    hr = dialog->Show(nullptr);
    if (FAILED(hr)) return std::nullopt;

    ComPtr<IShellItem> result;
    hr = dialog->GetResult(&result);
    if (FAILED(hr)) return std::nullopt;

    PWSTR folder_path = nullptr;
    hr = result->GetDisplayName(SIGDN_FILESYSPATH, &folder_path);
    if (FAILED(hr) || !folder_path) return std::nullopt;

    Path selected_path(to_string(folder_path));
    CoTaskMemFree(folder_path);
    return selected_path;
#else
    return std::nullopt;
#endif
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
    if (role == StatusRole::Warning) style = boxer::Style::Warning;
    else if (role == StatusRole::Error || role == StatusRole::Critical) style = boxer::Style::Error;

    boxer::show(message.c_str(), title.c_str(), style, boxer::Buttons::OK);
}
