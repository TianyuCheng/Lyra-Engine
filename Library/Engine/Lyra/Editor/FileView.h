#pragma once

#ifndef LYRA_LIBRARY_EDITOR_FILE_VIEW_H
#define LYRA_LIBRARY_EDITOR_FILE_VIEW_H

#include <Lyra/Common/Path.h>
#include <Lyra/Common/Promise.h>
#include <Lyra/Assets/AMSUtils.h>

// local imports
#include "../Runtime/Application.h"

namespace lyra
{
    struct AssetServer;

    struct FileView
    {
    public:
        explicit FileView(const Path& root);

        void bind(Application& app);

        void update(Blackboard& blackboard);

    private:
        // ui helpers
        void show_breadcrumb();
        void show_dir_files();
        void show_context_menu();
        void show_new_file_dialog();
        void show_new_folder_dialog();
        void show_import_indicator();
        void next_icon_grid(float row_width, float start_x);
        void draw_icon_grid(CString icon, CString text, float icon_scale) const;

    private:
        // data helpers
        void update_directory(const Path& path, bool force = false);
        void handle_file_drop(Blackboard& blackboard);

    private:
        struct Breadcrumb
        {
            String name;
            Path   path;
        };

    private:
        Path root;
        Path curr;

        Vector<String>          files       = {};
        Vector<String>          folders     = {};
        Vector<Breadcrumb>      breadcrumbs = {};
        Vector<Future<AssetID>> active_imports;
        uint32_t                finished_success = 0;
        uint32_t                finished_failure = 0;
        float                   notification_timer = 0.0f;

        bool show_new_file_modal   = false;
        bool show_new_folder_modal = false;

        float icon_size = 128.0f;
        float padding   = 16.0f;
    };
} // namespace lyra

#endif // LYRA_LIBRARY_EDITOR_FILE_VIEW_H
