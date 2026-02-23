#pragma once

#ifndef LYRA_LIBRARY_EDITOR_FILE_VIEW_H
#define LYRA_LIBRARY_EDITOR_FILE_VIEW_H

#include <Lyra/Common/Path.h>

// local imports
#include "../Runtime/Application.h"

namespace lyra
{
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
        void next_icon_grid(float row_width, float start_x);
        void draw_icon_grid(CString icon, CString text, float icon_scale) const;

    private:
        // data helpers
        void update_directory(const Path& path, bool force = false);

    private:
        Path root;
        Path curr;

        Vector<String> files   = {};
        Vector<String> folders = {};

        bool show_new_file_modal   = false;
        bool show_new_folder_modal = false;

        float icon_size = 128.0f;
        float padding   = 16.0f;
    };
} // namespace lyra

#endif // LYRA_LIBRARY_EDITOR_FILE_VIEW_H
