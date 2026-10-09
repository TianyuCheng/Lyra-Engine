#pragma once

#ifndef LYRA_ENGINE_UISYSTEM_WIDGETS_UIDIALOG_H
#define LYRA_ENGINE_UISYSTEM_WIDGETS_UIDIALOG_H

#include <optional>
#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Path.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Utilities/Function.h>
#include <Lyra/UISystem/Widgets/UIEnums.h>

namespace lyra::ui::dialog
{
    struct Filter
    {
        String name; // e.g. "Lyra Scene (*.lyra)"
        String spec; // e.g. "*.lyra;*.scene"
    };

    struct Options
    {
        String         title;
        Path           default_path;
        Vector<Filter> filters;
        bool           allow_multiple = false;
    };

    // =========================================================================
    // 1. Synchronous File Dialogs
    // =========================================================================

    auto open_file(const Options& options = {}) -> std::optional<Path>;
    auto open_files(const Options& options = {}) -> Vector<Path>;
    auto save_file(const Options& options = {}) -> std::optional<Path>;
    auto select_folder(const Options& options = {}) -> std::optional<Path>;

    // =========================================================================
    // 2. Asynchronous File Dialogs
    // =========================================================================

    using OnFileSelected  = Function<void(const std::optional<Path>&)>;
    using OnFilesSelected = Function<void(const Vector<Path>&)>;

    void open_file_async(const Options& options, OnFileSelected on_selected);
    void save_file_async(const Options& options, OnFileSelected on_selected);
    void select_folder_async(const Options& options, OnFileSelected on_selected);

    // =========================================================================
    // 3. User Alert & Confirmation
    // =========================================================================

    bool confirm(const String& title, const String& message);
    void alert(const String& title, const String& message, StatusRole role = StatusRole::Info);

} // namespace lyra::ui::dialog

#endif // LYRA_ENGINE_UISYSTEM_WIDGETS_UIDIALOG_H
