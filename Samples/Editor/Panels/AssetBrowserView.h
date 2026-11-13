#pragma once

#ifndef LYRA_EDITOR_PANELS_ASSET_BROWSER_VIEW_H
#define LYRA_EDITOR_PANELS_ASSET_BROWSER_VIEW_H

#include <Lyra/Utilities/Path.h>
#include <Lyra/Assets/AMSUtils.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/UISystem/Renderer/GUITypes.h>

#include "Common/SelectionModel.h"
#include <Lyra/Runtime/Application.h>

namespace lyra
{
    struct AssetServer;

    enum class AssetBrowserDomain
    {
        App, ///< Project Assets (app://)
        Sys  ///< Engine Assets (sys://)
    };

    struct AssetBrowserView
    {
    public:
        explicit AssetBrowserView(const Path& app_root, const Path& sys_root = {});
        virtual ~AssetBrowserView();

        void switch_domain(AssetBrowserDomain domain);
        void navigate_to(const Path& path, AssetBrowserDomain domain, bool record_history = true);
        bool can_go_back() const { return history_cursor > 0; }
        bool can_go_forward() const { return history_cursor + 1 < history.size(); }
        void action_navigate_back();
        void action_navigate_forward();

        bool is_read_only() const { return active_domain == AssetBrowserDomain::Sys; }
        auto get_current_scheme() const -> StringView;
        auto get_app_root() const -> const Path& { return app_root; }
        auto get_sys_root() const -> const Path& { return sys_root; }

        void bind(Application& app);

        void update(AppContext& context);

    private:
        // ui helpers
        void show_header_toolbar();
        void show_sidebar_tree();
        void show_tree_root(CString label, CString icon, const Path& root_path, AssetBrowserDomain domain);
        void show_directory_node(const Path& dir_path, const Path& domain_root, AssetBrowserDomain domain);
        void show_breadcrumb();
        void show_dir_files(AppContext& context);
        void show_item(AppContext& context, StringView name, bool is_folder);
        void show_context_menu(AppContext& context);
        void show_create_menu();
        void show_status_bar(AppContext& context);
        void show_modals(AppContext& context);

        // modals
        void show_new_scene_dialog();
        void show_new_file_dialog();
        void show_new_folder_dialog();
        void show_rename_dialog();
        void show_delete_dialog(AppContext& context);
        void show_import_indicator();
        void show_input_modal(CString title, bool* p_open, CString prompt, char* buffer, usize buffer_size, CString action_label, FunctionRef<void(StringView)> on_submit);

        // actions
        void action_create_scene(StringView name);
        void action_delete_selected();
        void action_rename(StringView old_name, StringView new_name);
        void action_create_folder(StringView name);
        void action_create_file(StringView name);
        void action_reimport_selected(AssetServer* ams);

    private:
        // data helpers
        void update_directory(const Path& path, bool force = false);
        void perform_update_directory(const Path& path, bool force = false);
        void handle_file_drop(AppContext& context);

        auto get_gui_renderer() const -> GUIRenderer*;
        auto get_asset_server() const -> AssetServer*;

        auto get_thumbnail(AppContext& context, StringView name) -> std::pair<GUITextureHandle, Vector2>;
        void load_thumbnails(AppContext& context);
        void load_editor_icons();

    private:
        struct Breadcrumb
        {
            String name;
            Path   path;
        };

        struct HistoryEntry
        {
            Path               path;
            AssetBrowserDomain domain;
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
        static auto create_texture_from_memory(const void* data, usize size, GUIRenderer* gui) -> ThumbnailTexture;

    private:
        AssetBrowserDomain active_domain = AssetBrowserDomain::App;
        Path               app_root;
        Path               sys_root;
        Path               root;
        Path               curr;
        AppContext*        context = nullptr;
        AssetServer*       ams     = nullptr;

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
        bool               needs_tree_reveal       = true;
        float              notification_timer      = 0.0f;

        SelectionModel selection;
        bool           is_marquee_selecting = false;
        Vector2        marquee_start_pos    = {0.0f, 0.0f};
        Vector<String> initial_selection    = {};

        ThumbnailTexture                  folder_icon;
        ThumbnailTexture                  file_icon;
        HashMap<String, ThumbnailTexture> thumbnails;
        Vector<std::pair<String, String>> queued_thumbnails;

        bool show_new_scene_modal  = false;
        bool show_new_file_modal   = false;
        bool show_new_folder_modal = false;
        bool show_delete_modal     = false;
        bool show_rename_modal     = false;

        bool open_new_scene_modal  = false;
        bool open_new_file_modal   = false;
        bool open_new_folder_modal = false;
        bool open_delete_modal     = false;
        bool open_rename_modal     = false;

        char  new_scene_name[256]  = "NewScene";
        char  new_file_name[256]   = "";
        char  new_folder_name[256] = "";
        char  rename_buffer[256]   = "";
        char  search_filter[256]   = "";
        float icon_size            = 128.0f;

        // sidebar and history state
        bool  show_sidebar  = true;
        float sidebar_width = 0.0f;

        static constexpr usize MAX_HISTORY    = 64;
        Vector<HistoryEntry>   history        = {};
        usize                  history_cursor = 0;

        Path next_path;
        bool needs_refresh = false;
        bool force_refresh = false;
    };
} // namespace lyra

#endif // LYRA_EDITOR_PANELS_ASSET_BROWSER_VIEW_H
