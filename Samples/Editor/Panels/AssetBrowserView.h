#pragma once

#ifndef LYRA_EDITOR_PANELS_ASSET_BROWSER_VIEW_H
#define LYRA_EDITOR_PANELS_ASSET_BROWSER_VIEW_H

#include <Lyra/Utilities/Path.h>
#include <Lyra/Assets/AMSUtils.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/UISystem/GUITypes.h>

#include "Common/SelectionModel.h"
#include <Lyra/Runtime/Application.h>

namespace lyra
{
    struct AssetServer;

    struct AssetBrowserView
    {
    public:
        explicit AssetBrowserView(const Path& root);
        virtual ~AssetBrowserView();

        void bind(Application& app);

        void update(Blackboard& blackboard);

    private:
        // ui helpers
        void show_header_toolbar();
        void show_breadcrumb();
        void show_dir_files(Blackboard& blackboard);
        void show_item(Blackboard& blackboard, StringView name, bool is_folder);
        void show_context_menu(Blackboard& blackboard);
        void show_create_menu();
        void show_status_bar(Blackboard& blackboard);
        void show_modals(Blackboard& blackboard);

        // modals
        void show_new_file_dialog();
        void show_new_folder_dialog();
        void show_rename_dialog();
        void show_delete_dialog(Blackboard& blackboard);
        void show_import_indicator();
        void show_input_modal(CString title, bool* p_open, CString prompt, char* buffer, size_t buffer_size, CString action_label, FunctionRef<void(StringView)> on_submit);

        // actions
        void action_delete_selected();
        void action_rename(StringView old_name, StringView new_name);
        void action_create_folder(StringView name);
        void action_create_file(StringView name);
        void action_reimport_selected(AssetServer* ams);

    private:
        // data helpers
        void update_directory(const Path& path, bool force = false);
        void perform_update_directory(const Path& path, bool force = false);
        void handle_file_drop(Blackboard& blackboard);

        auto get_gui_renderer() const -> GUIRenderer*;
        auto get_asset_server() const -> AssetServer*;

        auto get_thumbnail(Blackboard& blackboard, StringView name) -> std::pair<GUITextureHandle, Vector2>;
        void load_thumbnails(Blackboard& blackboard);
        void load_editor_icons();

    private:
        struct Breadcrumb
        {
            String name;
            Path   path;
        };

        struct ThumbnailTexture
        {
            GPUTexture texture;
            GUITexture gui_texture;
            bool       valid = false;
        };

        struct TextureUploadEntry
        {
            String   name;
            int      width  = 0;
            int      height = 0;
            uint8_t* pixels = nullptr;
        };

        static auto upload_rgba_textures(Vector<TextureUploadEntry>& entries, GUIRenderer* gui)
            -> Vector<std::pair<String, ThumbnailTexture>>;
        static auto create_texture_from_memory(const void* data, size_t size, GUIRenderer* gui) -> ThumbnailTexture;

    private:
        Path        root;
        Path        curr;
        Blackboard* bboard = nullptr;

        Vector<String>     files                   = {};
        Vector<String>     folders                 = {};
        Vector<String>     all_items               = {}; // cached combined list
        Vector<Breadcrumb> breadcrumbs             = {};
        uint32_t           session_success         = 0;
        uint32_t           session_failure         = 0;
        uint32_t           session_start_completed = 0;
        uint32_t           session_start_failed    = 0;
        uint32_t           last_completed_cooks    = 0;
        bool               was_cooking             = false;
        float              notification_timer      = 0.0f;

        SelectionModel selection;
        bool           is_marquee_selecting = false;
        Vector2        marquee_start_pos    = {0.0f, 0.0f};
        Vector<String> initial_selection    = {};

        ThumbnailTexture                  folder_icon;
        ThumbnailTexture                  file_icon;
        HashMap<String, ThumbnailTexture> thumbnails;
        Vector<std::pair<String, String>> queued_thumbnails;

        bool show_new_file_modal   = false;
        bool show_new_folder_modal = false;
        bool show_delete_modal     = false;
        bool show_rename_modal     = false;

        bool open_new_file_modal   = false;
        bool open_new_folder_modal = false;
        bool open_delete_modal     = false;
        bool open_rename_modal     = false;

        char new_file_name[256]   = "";
        char new_folder_name[256] = "";
        char rename_buffer[256]   = "";
        char search_filter[256]   = "";
        float icon_size           = 96.0f;

        Path next_path;
        bool needs_refresh = false;
        bool force_refresh = false;
    };
} // namespace lyra

#endif // LYRA_EDITOR_PANELS_ASSET_BROWSER_VIEW_H
