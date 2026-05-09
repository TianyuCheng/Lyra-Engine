#pragma once

#ifndef LYRA_LYRA_EDITOR_FILE_VIEW_H
#define LYRA_LYRA_EDITOR_FILE_VIEW_H

#include <Lyra/Common/Path.h>
#include <Lyra/Common/Promise.h>
#include <Lyra/Assets/AMSUtils.h>

// local imports
#include <Lyra/Editor/IconGrid.h>
#include <Lyra/Editor/SelectionModel.h>
#include <Lyra/Player/Application.h>

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
        void show_dir_files(Blackboard& blackboard);
        void show_item(Blackboard& blackboard, IconGrid& grid, IconGrid::Context& ctx, StringView name, bool is_folder);
        void show_context_menu(Blackboard& blackboard);

        // modals
        void show_new_file_dialog();
        void show_new_folder_dialog();
        void show_rename_dialog();
        void show_delete_dialog(Blackboard& blackboard);
        void show_import_indicator();

        // actions
        void action_delete_selected();
        void action_rename(StringView old_name, StringView new_name);
        void action_create_folder(StringView name);
        void action_reimport_selected(AssetServer* ams);

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
        Vector<String>          all_items   = {}; // Cached combined list
        Vector<Breadcrumb>      breadcrumbs = {};
        Vector<Future<AssetID>> active_imports;
        uint                    finished_success   = 0;
        uint                    finished_failure   = 0;
        float                   notification_timer = 0.0f;

        SelectionModel selection;
        IconGrid       grid;

        bool           is_marquee_selecting = false;
        ImVec2         marquee_start_pos;
        Vector<String> initial_selection; // selection state before marquee started

        bool show_new_file_modal   = false;
        bool show_new_folder_modal = false;
        bool show_delete_modal     = false;
        bool show_rename_modal     = false;

        char new_folder_name[256] = "";
        char rename_buffer[256]   = "";
    };
} // namespace lyra

#endif // LYRA_LYRA_EDITOR_FILE_VIEW_H
