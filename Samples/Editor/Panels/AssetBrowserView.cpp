#include <fstream>
#include <algorithm>
#include <stb_image.h>
#include <cmrc/cmrc.hpp>
#include <Lyra/Common/Path.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Assets/AMSServer.h>
#include <Lyra/Render/RHITypes.h>
#include <Lyra/Render/RHIInits.h>
#include <Lyra/UICore/UI.h>
#include <Lyra/UICore/UILayout.h>
#include <Lyra/UICore/UIControls.h>
#include <Lyra/UICore/UIDock.h>

// local imports
#include <Lyra/UICore/UIIcons.h>
#include "AssetBrowserView.h"

CMRC_DECLARE(editor);

#define LYRA_FILES_WINDOW_NAME (LYRA_ICON_FOLDER " Files")

using namespace lyra;

AssetBrowserView::AssetBrowserView(const Path& root)
    : root(root), curr(root)
{
    assert(std::filesystem::exists(root));
    assert(std::filesystem::is_directory(root));
}

AssetBrowserView::~AssetBrowserView()
{
    if (bboard && bboard->has<GUIRenderer*>()) {
        auto gui = bboard->get<GUIRenderer*>();
        if (folder_icon.valid) {
            gui->delete_texture(folder_icon.gui_texture);
            folder_icon.valid = false;
        }
        if (file_icon.valid) {
            gui->delete_texture(file_icon.gui_texture);
            file_icon.valid = false;
        }
        for (auto& [name, thumb] : thumbnails) {
            if (thumb.valid) {
                gui->delete_texture(thumb.gui_texture);
            }
        }
    }
    thumbnails.clear();
}

void AssetBrowserView::bind(Application& app)
{
    bboard = &app.get_blackboard();
    if (auto ams_ptr = app.get_blackboard().try_get<AssetServer*>()) {
        last_completed_cooks = (*ams_ptr)->get_pipeline_stats().completed_count;
        (*ams_ptr)->set_on_filesystem_changed([this]() {
            needs_refresh = true;
            force_refresh = true;
        });
    }
    app.bind<AppEvent::UPDATE, &AssetBrowserView::update>(*this);
    update_directory(root, true);
    load_editor_icons();
}

void AssetBrowserView::update(Blackboard& blackboard)
{
    if ((!folder_icon.valid || !file_icon.valid) && bboard && bboard->has<GUIRenderer*>()) {
        load_editor_icons();
    }

    if (bboard) {
        if (auto ams = bboard->try_get<AssetServer*>()) {
            auto stats = (*ams)->get_pipeline_stats();
            if (stats.completed_count != last_completed_cooks) {
                last_completed_cooks = stats.completed_count;
                // invalidate failed/pending thumbnail cache entries so newly generated .import thumbnails load automatically
                for (auto it = thumbnails.begin(); it != thumbnails.end();) {
                    if (!it->second.valid) {
                        thumbnails.erase(it++);
                    } else {
                        ++it;
                    }
                }
            }
        }
    }

    if (needs_refresh) {
        perform_update_directory(next_path, force_refresh);
        needs_refresh = false;
        force_refresh = false;
    }

    lyra::execute_once([&]() {
        ui::workspace::dock(LYRA_FILES_WINDOW_NAME, ui::Area::Bottom);
    });

    ui::panel(LYRA_FILES_WINDOW_NAME, [&]() {
        handle_file_drop(blackboard);

        ui::toolbar([&]() {
            show_breadcrumb();
            ui::spacer();
            ui::search_bar(search_filter, sizeof(search_filter), 250.0f);
        });

        ui::separator();

        ui::scroll_area("FileBrowser", 32.0f, [&]() {
            if (!selection.empty() && !show_rename_modal && !show_new_file_modal && !show_new_folder_modal && !show_delete_modal) {
                if (ui::is_panel_hovered() && !ui::is_any_item_active() && (ui::is_key_pressed(KeyButton::DEL) || ui::is_key_pressed(KeyButton::BACKSPACE))) {
                    show_delete_modal = true;
                    open_delete_modal = true;
                }
            }

            show_dir_files(blackboard);
            show_context_menu(blackboard);

            show_new_file_dialog();
            show_new_folder_dialog();
            show_rename_dialog();
            show_delete_dialog(blackboard);
        });

        ui::separator();
        ui::row([&]() {
            char count_buf[128];
            snprintf(count_buf, sizeof(count_buf), " %zu items  |  %zu selected", files.size() + folders.size(), selection.size());
            ui::label(count_buf, ui::StatusRole::Muted);

            ui::spacer();

            if (auto ams_ptr = blackboard.try_get<AssetServer*>()) {
                auto ams   = *ams_ptr;
                auto stats = ams->get_pipeline_stats();
                char status_buf[256];
                snprintf(status_buf, sizeof(status_buf), "[Assets: %s | %u Cooked]",
                    stats.watching ? "Watching" : "Idle", stats.completed_count);
                ui::label(status_buf, stats.watching ? ui::StatusRole::Success : ui::StatusRole::Muted);
            }

            show_import_indicator();

            ui::separator();
            ui::slider("icon_size", &icon_size, 64.0f, 128.0f, "%.0f px", 100.0f);
        });
    });
}

// --- UI Helpers ---

void AssetBrowserView::show_breadcrumb()
{
    Vector<ui::BreadcrumbItem> items;
    items.reserve(breadcrumbs.size() + 1);

    String root_name = root.filename().string();
    if (root_name.empty()) root_name = "Assets";

    bool is_at_root = (curr == root);
    items.push_back({
        root_name.c_str(),
        LYRA_ICON_HOME,
        [&]() { update_directory(root, is_at_root); },
        is_at_root ? "Root directory (Click to refresh)" : "Go to Root"
    });

    for (size_t i = 0; i < breadcrumbs.size(); ++i) {
        const auto& bc      = breadcrumbs[i];
        bool        is_last = (i == breadcrumbs.size() - 1);
        items.push_back({
            bc.name.c_str(),
            is_last ? LYRA_ICON_FOLDER : nullptr,
            [&bc, is_last, this]() { update_directory(bc.path, is_last); },
            is_last ? "Current directory (Click to refresh)" : nullptr
        });
    }

    ui::breadcrumb(items);
}

void AssetBrowserView::show_dir_files(Blackboard& blackboard)
{
    // explorer / finder style background click logic:
    // when clicking on empty background (no item hovered or active), deselect
    if (ui::is_panel_hovered() && !ui::is_any_item_hovered() && !ui::is_any_item_active()) {
        if (ui::is_mouse_clicked(MouseButton::LEFT)) {
            if (!ui::is_key_down(KeyButton::CTRL)) {
                selection.clear();
            }
        }
    }

    String filter(search_filter);
    auto   matches_filter = [&](StringView name) {
        if (filter.empty()) return true;
        String n(name);
        std::transform(n.begin(), n.end(), n.begin(), ::tolower);
        String f(filter);
        std::transform(f.begin(), f.end(), f.begin(), ::tolower);
        return n.find(f) != String::npos;
    };

    ui::grid("files_grid", icon_size + 4.0f, [&]() {
        for (const auto& folder : folders) {
            if (!matches_filter(folder)) continue;
            ui::grid_item([&]() {
                show_item(blackboard, folder, true);
            });
        }

        for (const auto& file : files) {
            if (!matches_filter(file)) continue;
            ui::grid_item([&]() {
                show_item(blackboard, file, false);
            });
        }
    });

    load_thumbnails(blackboard);
}

void AssetBrowserView::show_item(Blackboard& blackboard, StringView name, bool is_folder)
{
    bool is_sel = selection.is_selected(name);

    auto on_click = [&]() {
        if (ui::is_key_down(KeyButton::CTRL))
            selection.toggle(name);
        else
            selection.select_only(name);
    };

    if (is_folder) {
        if (folder_icon.valid) {
            ui::card(name.data(), folder_icon.gui_texture.texid,
                Vector2((float)folder_icon.texture.width, (float)folder_icon.texture.height),
                name.data(), is_sel, on_click, [&]() {
                update_directory(curr / name);
            }, icon_size);
        } else {
            ui::card(name.data(), LYRA_ICON_FOLDER, name.data(), is_sel, on_click, [&]() {
                update_directory(curr / name);
            }, Vector4(1.0f, 0.75f, 0.25f, 1.0f), icon_size);
        }
    } else {
        auto [id, size] = get_thumbnail(blackboard, name);
        if (id != GUITextureHandle{}) {
            ui::card(name.data(), id, size, name.data(), is_sel, on_click, icon_size);
        } else if (file_icon.valid) {
            ui::card(name.data(), file_icon.gui_texture.texid,
                Vector2((float)file_icon.texture.width, (float)file_icon.texture.height),
                name.data(), is_sel, on_click, icon_size);
        } else {
            ui::card(name.data(), LYRA_ICON_FILE, name.data(), is_sel, on_click, Vector4(0.0f), icon_size);
        }
    }

    ui::item_context_menu([&]() {
        if (!is_sel) selection.select_only(name);

        if (selection.size() == 1) {
            ui::menu_item(LYRA_ICON_RENAME " Rename", [&]() {
                show_rename_modal = true;
                open_rename_modal = true;
                strncpy_s(rename_buffer, sizeof(rename_buffer), selection.items[0].c_str(), sizeof(rename_buffer) - 1);
            });
        }
        if (!is_folder) {
            ui::menu_item(LYRA_ICON_IMPORT " Re-import", [&]() {
                action_reimport_selected(blackboard.get<AssetServer*>());
            });
        }
        ui::menu_item(LYRA_ICON_DELETE " Delete", [&]() {
            show_delete_modal = true;
            open_delete_modal = true;
        });
        ui::separator();
        ui::menu(LYRA_ICON_NEW_FILE " Create", [&]() {
            ui::menu_item(LYRA_ICON_NEW_FILE " Create File", [&]() {
                new_file_name[0]    = '\0';
                show_new_file_modal = true;
                open_new_file_modal = true;
            });
            ui::menu_item(LYRA_ICON_NEW_FOLDER " Create Folder", [&]() {
                new_folder_name[0]    = '\0';
                show_new_folder_modal = true;
                open_new_folder_modal = true;
            });
        });
    });
}

void AssetBrowserView::show_context_menu(Blackboard& blackboard)
{
    ui::panel_context_menu([&]() {
        ui::menu_item(LYRA_ICON_REFRESH " Refresh", [&]() {
            update_directory(curr, true);
        });
        ui::separator();
        ui::menu_item(LYRA_ICON_IMPORT " Import", [&]() {
            spdlog::info("Import not implemented");
        });
        ui::separator();
        ui::menu(LYRA_ICON_NEW_FILE " Create", [&]() {
            ui::menu_item(LYRA_ICON_NEW_FILE " Create File", [&]() {
                new_file_name[0]    = '\0';
                show_new_file_modal = true;
                open_new_file_modal = true;
            });
            ui::menu_item(LYRA_ICON_NEW_FOLDER " Create Folder", [&]() {
                new_folder_name[0]    = '\0';
                show_new_folder_modal = true;
                open_new_folder_modal = true;
            });
        });
    });

    if (open_new_file_modal) {
        ui::open_modal(LYRA_ICON_NEW_FILE " New File");
        open_new_file_modal = false;
    }
    if (open_new_folder_modal) {
        ui::open_modal(LYRA_ICON_NEW_FOLDER " New Folder");
        open_new_folder_modal = false;
    }
    if (open_delete_modal) {
        ui::open_modal(LYRA_ICON_DELETE " Delete");
        open_delete_modal = false;
    }
    if (open_rename_modal) {
        ui::open_modal(LYRA_ICON_RENAME " Rename");
        open_rename_modal = false;
    }
}

// --- Actions ---

void AssetBrowserView::action_delete_selected()
{
    AssetServer* ams = nullptr;
    if (bboard && bboard->has<AssetServer*>()) {
        ams = bboard->get<AssetServer*>();
    }

    for (const auto& target : selection.items) {
        Path p   = curr / target;
        Path rel = std::filesystem::relative(p, root);
        bool deleted = false;
        if (ams) {
            deleted = ams->delete_asset(rel);
        }
        if (!deleted || std::filesystem::exists(p)) {
            Path import_p = p;
            import_p += ".import";
            try {
                if (std::filesystem::exists(p)) std::filesystem::remove_all(p);
            } catch (const std::exception& e) {
                spdlog::error("Failed to delete {}: {}", p.string(), e.what());
            }
            try {
                if (std::filesystem::exists(import_p)) std::filesystem::remove_all(import_p);
            } catch (const std::exception& e) {
                spdlog::error("Failed to delete {}: {}", import_p.string(), e.what());
            }
        }
        auto it = thumbnails.find(target);
        if (it != thumbnails.end()) {
            if (it->second.valid && bboard && bboard->has<GUIRenderer*>()) {
                auto gui = bboard->get<GUIRenderer*>();
                gui->delete_texture(it->second.gui_texture);
            }
            thumbnails.erase(it);
        }
    }
    selection.clear();
    update_directory(curr, true);
}

void AssetBrowserView::action_rename(StringView old_name, StringView new_name)
{
    if (old_name == new_name) return;
    Path op = curr / old_name;
    Path np = curr / new_name;

    AssetServer* ams = nullptr;
    if (bboard && bboard->has<AssetServer*>()) {
        ams = bboard->get<AssetServer*>();
    }

    if (ams) {
        Path rel_op = std::filesystem::relative(op, root);
        Path rel_np = std::filesystem::relative(np, root);
        if (!ams->move_asset(rel_op, rel_np)) {
            spdlog::error("Failed to move/rename asset: {} to {}", op.string(), np.string());
        }
    } else {
        try {
            if (std::filesystem::exists(np)) {
                spdlog::error("Rename failed: Destination exists");
            } else {
                std::filesystem::rename(op, np);
                Path old_import = op;
                old_import += ".import";
                Path new_import = np;
                new_import += ".import";
                if (std::filesystem::exists(old_import)) {
                    std::filesystem::rename(old_import, new_import);
                }
            }
        } catch (const std::exception& e) {
            spdlog::error("Rename error: {}", e.what());
        }
    }
    update_directory(curr, true);
}

void AssetBrowserView::action_create_folder(StringView name)
{
    auto trim = [](StringView s) -> StringView {
        size_t first = s.find_first_not_of(" \t\n\r");
        if (first == StringView::npos) return "";
        size_t last = s.find_last_not_of(" \t\n\r");
        return s.substr(first, (last - first + 1));
    };
    StringView trimmed = trim(name);
    if (trimmed.empty()) return;

    Path p = curr / trimmed;
    try {
        if (std::filesystem::exists(p)) {
            spdlog::error("Folder create error: Destination exists ({})", p.string());
        } else if (std::filesystem::create_directory(p)) {
            update_directory(curr, true);
        }
    } catch (const std::exception& e) {
        spdlog::error("Folder create error: {}", e.what());
    }
}

void AssetBrowserView::action_create_file(StringView name)
{
    auto trim = [](StringView s) -> StringView {
        size_t first = s.find_first_not_of(" \t\n\r");
        if (first == StringView::npos) return "";
        size_t last = s.find_last_not_of(" \t\n\r");
        return s.substr(first, (last - first + 1));
    };
    StringView trimmed = trim(name);
    if (trimmed.empty()) return;

    Path p = curr / trimmed;
    try {
        if (std::filesystem::exists(p)) {
            spdlog::error("File create error: Destination exists ({})", p.string());
        } else {
            std::ofstream f(p);
            if (f.is_open()) {
                f.close();
                update_directory(curr, true);
            } else {
                spdlog::error("Failed to create file: {}", p.string());
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("File create error: {}", e.what());
    }
}

void AssetBrowserView::action_reimport_selected(AssetServer* ams)
{
    if (!ams) return;
    auto gui = (bboard && bboard->has<GUIRenderer*>()) ? bboard->get<GUIRenderer*>() : nullptr;
    for (const auto& sel : selection.items) {
        if (std::find(files.begin(), files.end(), sel) != files.end()) {
            ams->import_asset(std::filesystem::relative(curr / sel, root), true);
            auto it = thumbnails.find(sel);
            if (it != thumbnails.end()) {
                if (it->second.valid && gui) {
                    gui->delete_texture(it->second.gui_texture);
                }
                thumbnails.erase(it);
            }
        }
    }
}

// --- Modals ---

void AssetBrowserView::show_new_folder_dialog()
{
    ui::modal(LYRA_ICON_NEW_FOLDER " New Folder", &show_new_folder_modal, [&]() {
        ui::label("Enter folder name:");
        ui::text_field("folder_name", new_folder_name, sizeof(new_folder_name), [&]() {
            action_create_folder(new_folder_name);
            show_new_folder_modal = false;
            ui::close_modal();
        });

        ui::separator();

        ui::row(ui::Alignment::End, [&]() {
            ui::button("Cancel", [&]() {
                show_new_folder_modal = false;
                ui::close_modal();
            });

            ui::button("Create", [&]() {
                action_create_folder(new_folder_name);
                show_new_folder_modal = false;
                ui::close_modal();
            }, ui::ButtonRole::Primary);
        });
    });
}

void AssetBrowserView::show_rename_dialog()
{
    ui::modal(LYRA_ICON_RENAME " Rename", &show_rename_modal, [&]() {
        ui::label("Enter new name:");
        ui::text_field("rename_buffer", rename_buffer, sizeof(rename_buffer), [&]() {
            action_rename(selection.items[0], rename_buffer);
            show_rename_modal = false;
            ui::close_modal();
        });

        ui::separator();

        ui::row(ui::Alignment::End, [&]() {
            ui::button("Cancel", [&]() {
                show_rename_modal = false;
                ui::close_modal();
            });

            ui::button("Rename", [&]() {
                action_rename(selection.items[0], rename_buffer);
                show_rename_modal = false;
                ui::close_modal();
            }, ui::ButtonRole::Primary);
        });
    });
}

void AssetBrowserView::show_delete_dialog(Blackboard&)
{
    ui::modal(LYRA_ICON_DELETE " Delete", &show_delete_modal, [&]() {
        if (selection.size() == 1) {
            ui::label("Are you sure you want to delete:");
            ui::label(selection.items[0].c_str(), ui::StatusRole::Error);
        } else {
            char del_buf[128];
            snprintf(del_buf, sizeof(del_buf), "Are you sure you want to delete %zu selected items?", selection.size());
            ui::label(del_buf);
        }

        ui::label("This action cannot be undone.", ui::StatusRole::Muted);
        ui::separator();

        ui::row(ui::Alignment::End, [&]() {
            ui::button("Cancel", [&]() {
                show_delete_modal = false;
                ui::close_modal();
            });

            ui::button("Delete", [&]() {
                action_delete_selected();
                show_delete_modal = false;
                ui::close_modal();
            }, ui::ButtonRole::Danger);
        });
    });
}

void AssetBrowserView::update_directory(const Path& path, bool force)
{
    next_path     = path;
    needs_refresh = true;
    force_refresh = force;
}

void AssetBrowserView::perform_update_directory(const Path& path, bool force)
{
    if (!force && curr == path) return;
    curr = path;
    files.clear();
    folders.clear();
    all_items.clear();
    breadcrumbs.clear();
    selection.clear();

    if (bboard && bboard->has<GUIRenderer*>()) {
        auto gui = bboard->get<GUIRenderer*>();
        for (auto& [name, thumb] : thumbnails) {
            if (thumb.valid) {
                gui->delete_texture(thumb.gui_texture);
            }
        }
    }
    thumbnails.clear();

    auto relative = std::filesystem::relative(curr, root);
    Path b_path   = root;
    for (const auto& part : relative) {
        if (part == ".") continue;
        b_path /= part;
        breadcrumbs.push_back({to_string(part), b_path});
    }

    for (const auto& entry : std::filesystem::directory_iterator(curr)) {
        if (entry.path().extension() == ".import") continue;
        String rel = to_string(std::filesystem::relative(entry.path(), curr));
        if (entry.is_directory())
            folders.push_back(rel);
        else
            files.push_back(rel);
    }

    // sort and build all_items
    std::sort(folders.begin(), folders.end());
    std::sort(files.begin(), files.end());

    all_items.reserve(folders.size() + files.size());
    for (const auto& f : folders)
        all_items.push_back(f);
    for (const auto& f : files)
        all_items.push_back(f);
}

void AssetBrowserView::handle_file_drop(Blackboard& blackboard)
{
    auto window = blackboard.get<Window*>();

    if (!ui::is_panel_hovered())
        return;

    if (!window->get_input_state().has_dropped_files())
        return;

    for (const auto& path_str : window->get_input_state().get_dropped_files()) {
        Path src(path_str);
        Path dst = curr / src.filename();
        try {
            std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing);
            spdlog::info("Copied dropped file to: {}", dst.string());
        } catch (const std::exception& e) {
            spdlog::error("Failed to copy dropped file {}: {}", path_str, e.what());
        }
    }
    update_directory(curr, true);
}

void AssetBrowserView::show_import_indicator()
{
    AssetPipelineStats stats{};
    if (bboard) {
        if (auto ams = bboard->try_get<AssetServer*>()) {
            stats = (*ams)->get_pipeline_stats();
        }
    }

    if (stats.pending_count > 0) {
        if (!was_cooking) {
            was_cooking             = true;
            session_success         = 0;
            session_failure         = 0;
            session_start_completed = stats.completed_count;
            session_start_failed    = stats.failed_count;
        }
        notification_timer = 0.0f;
    } else if (was_cooking) {
        was_cooking        = false;
        session_success    = stats.completed_count >= session_start_completed ? (stats.completed_count - session_start_completed) : 0;
        session_failure    = stats.failed_count >= session_start_failed ? (stats.failed_count - session_start_failed) : 0;
        notification_timer = 5.0f;
        for (auto it = thumbnails.begin(); it != thumbnails.end();) {
            if (!it->second.valid) {
                thumbnails.erase(it++);
            } else {
                ++it;
            }
        }
    } else if (notification_timer > 0.0f) {
        notification_timer -= 0.016f;
    }

    if (!was_cooking && notification_timer <= 0.0f)
        return;

    if (was_cooking) {
        char buf[128];
        if (!stats.current_asset.empty()) {
            snprintf(buf, sizeof(buf), LYRA_ICON_IMPORT " Cooking %u (%s)...", stats.pending_count, stats.current_asset.c_str());
        } else {
            snprintf(buf, sizeof(buf), LYRA_ICON_IMPORT " Cooking %u asset%s...", stats.pending_count, stats.pending_count > 1 ? "s" : "");
        }
        ui::badge(buf, ui::StatusRole::Info);
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "Import finished: %u ok, %u failed", session_success, session_failure);
        ui::badge(buf, session_failure > 0 ? ui::StatusRole::Error : ui::StatusRole::Success);
    }
}

void AssetBrowserView::show_new_file_dialog()
{
    ui::modal(LYRA_ICON_NEW_FILE " New File", &show_new_file_modal, [&]() {
        ui::label("Enter file name:");
        ui::text_field("file_name", new_file_name, sizeof(new_file_name), [&]() {
            action_create_file(new_file_name);
            show_new_file_modal = false;
            ui::close_modal();
        });

        ui::separator();

        ui::row(ui::Alignment::End, [&]() {
            ui::button("Cancel", [&]() {
                show_new_file_modal = false;
                ui::close_modal();
            });

            ui::button("Create", [&]() {
                action_create_file(new_file_name);
                show_new_file_modal = false;
                ui::close_modal();
            }, ui::ButtonRole::Primary);
        });
    });
}

std::pair<GUITextureHandle, Vector2> AssetBrowserView::get_thumbnail(Blackboard& blackboard, StringView name)
{
    auto it = thumbnails.find(String(name));
    if (it != thumbnails.end()) {
        if (it->second.valid) {
            return {it->second.gui_texture.texid,
                Vector2((float)it->second.texture.width, (float)it->second.texture.height)};
        }
        return {GUITextureHandle{}, Vector2(0.0f, 0.0f)};
    }

    // not in cache, check if we should queue it
    auto name_str = String(name);
    auto q_it     = std::find_if(queued_thumbnails.begin(), queued_thumbnails.end(), [&](const auto& p) {
        return p.first == name_str;
    });

    if (q_it == queued_thumbnails.end()) {
        // not in queue, check .import file
        Path import_path = curr / (name_str + ".import");
        if (std::filesystem::exists(import_path)) {
            try {
                std::ifstream f(import_path);
                JSON          j = JSON::parse(f);
                if (j.contains("thumbnail") && j["thumbnail"].is_string()) {
                    queued_thumbnails.push_back({name_str, j["thumbnail"].get<String>()});
                } else {
                    // mark as invalid so we don't check again this session
                    thumbnails[name_str] = {{}, {}, false};
                }
            } catch (...) {
                thumbnails[name_str] = {{}, {}, false};
            }
        } else {
            thumbnails[name_str] = {{}, {}, false};
        }
    }

    return {GUITextureHandle{}, Vector2(0.0f, 0.0f)};
}

void AssetBrowserView::load_thumbnails(Blackboard& blackboard)
{
    if (queued_thumbnails.empty()) return;

    auto  loader  = blackboard.get<FileLoader*>();
    auto& device  = RHI::get_current_device();
    auto& adapter = RHI::get_current_adapter();
    auto  gui     = blackboard.get<GUIRenderer*>();

    struct PendingThumb
    {
        String   name;
        int      w, h;
        stbi_uc* data;
        uint     row_pitch;
        uint     buffer_offset;
    };

    Vector<PendingThumb> pending;

    uint staging_size = 0;
    uint alignment    = adapter.properties.texture_row_pitch_alignment;
    for (const auto& [name, path] : queued_thumbnails) {
        if (!loader->exists(path.c_str())) {
            thumbnails[name] = {{}, {}, false};
            continue;
        }
        auto content = loader->read<uint8_t>(path.c_str());
        if (content.empty()) {
            thumbnails[name] = {{}, {}, false};
            continue;
        }

        int      w = 0, h = 0, c = 0;
        stbi_uc* data = stbi_load_from_memory(content.data(), (int)content.size(), &w, &h, &c, STBI_rgb_alpha);
        if (!data || w <= 0 || h <= 0) {
            if (data) stbi_image_free(data);
            thumbnails[name] = {{}, {}, false};
            continue;
        }

        uint row_pitch = (w * 4 + alignment - 1) & ~(alignment - 1);
        uint img_size  = row_pitch * h;
        uint offset    = (staging_size + 255) & ~255; // 256 byte alignment

        pending.push_back({name, w, h, data, row_pitch, offset});
        staging_size = offset + img_size;
    }

    if (pending.empty()) {
        queued_thumbnails.clear();
        return;
    }

    GPUBufferDescriptor buf_desc{};
    buf_desc.size     = staging_size;
    buf_desc.usage    = GPUBufferUsage::COPY_SRC | GPUBufferUsage::MAP_WRITE;
    GPUBuffer staging = device.create_buffer(buf_desc);

    staging.map(GPUMapMode::WRITE);
    auto mapped = staging.get_mapped_range();
    for (const auto& p : pending) {
        for (int i = 0; i < p.h; i++) {
            std::memcpy(mapped.data + p.buffer_offset + p.row_pitch * i, p.data + p.w * 4 * i, p.w * 4);
        }
    }
    staging.unmap();

    GPUCommandBuffer cmdbuffer = execute([&]() {
        auto desc = GPUCommandBufferDescriptor{};
        return device.create_command_buffer(desc);
    });

    for (const auto& p : pending) {
        GPUTextureDescriptor tex_desc{};
        tex_desc.size      = {(uint)p.w, (uint)p.h, 1};
        tex_desc.format    = GPUTextureFormat::RGBA8UNORM;
        tex_desc.usage     = GPUTextureUsage::COPY_DST | GPUTextureUsage::TEXTURE_BINDING;
        GPUTexture texture = device.create_texture(tex_desc);

        GPUTexelCopyBufferInfo src_info{};
        src_info.buffer         = staging;
        src_info.offset         = p.buffer_offset;
        src_info.bytes_per_row  = p.row_pitch;
        src_info.rows_per_image = p.h;

        GPUTexelCopyTextureInfo dst_info{};
        dst_info.texture = texture;
        dst_info.aspect  = GPUTextureAspect::COLOR;

        cmdbuffer.resource_barrier(state_transition(texture, undefined_state(), copy_dst_state()));
        cmdbuffer.copy_buffer_to_texture(src_info, dst_info, {(uint)p.w, (uint)p.h, 1});
        cmdbuffer.resource_barrier(state_transition(texture, copy_dst_state(), shader_resource_state(GPUBarrierSync::PIXEL_SHADING)));

        GUITexture gui_tex = gui->create_texture(texture, texture.create_view());
        thumbnails[p.name] = {texture, gui_tex, true};

        stbi_image_free(p.data);
    }

    cmdbuffer.submit();
    device.wait();

    staging.destroy();
    queued_thumbnails.clear();
}

auto AssetBrowserView::create_texture_from_memory(const void* data, size_t size, GUIRenderer* gui) -> ThumbnailTexture
{
    if (!data || size == 0 || !gui) {
        return {};
    }

    int      w = 0, h = 0, c = 0;
    stbi_uc* pixels = stbi_load_from_memory(static_cast<const stbi_uc*>(data), static_cast<int>(size), &w, &h, &c, STBI_rgb_alpha);
    if (!pixels || w <= 0 || h <= 0) {
        if (pixels) stbi_image_free(pixels);
        return {};
    }

    auto& device  = RHI::get_current_device();
    auto& adapter = RHI::get_current_adapter();

    uint alignment = adapter.properties.texture_row_pitch_alignment;
    uint row_pitch = (w * 4 + alignment - 1) & ~(alignment - 1);
    uint img_size  = row_pitch * h;

    GPUBufferDescriptor buf_desc{};
    buf_desc.size     = img_size;
    buf_desc.usage    = GPUBufferUsage::COPY_SRC | GPUBufferUsage::MAP_WRITE;
    GPUBuffer staging = device.create_buffer(buf_desc);

    staging.map(GPUMapMode::WRITE);
    auto mapped = staging.get_mapped_range();
    for (int i = 0; i < h; i++) {
        std::memcpy(mapped.data + row_pitch * i, pixels + w * 4 * i, w * 4);
    }
    staging.unmap();

    GPUCommandBuffer cmdbuffer = execute([&]() {
        auto desc = GPUCommandBufferDescriptor{};
        return device.create_command_buffer(desc);
    });

    GPUTextureDescriptor tex_desc{};
    tex_desc.size      = {(uint)w, (uint)h, 1};
    tex_desc.format    = GPUTextureFormat::RGBA8UNORM;
    tex_desc.usage     = GPUTextureUsage::COPY_DST | GPUTextureUsage::TEXTURE_BINDING;
    GPUTexture texture = device.create_texture(tex_desc);

    GPUTexelCopyBufferInfo src_info{};
    src_info.buffer         = staging;
    src_info.offset         = 0;
    src_info.bytes_per_row  = row_pitch;
    src_info.rows_per_image = (uint)h;

    GPUTexelCopyTextureInfo dst_info{};
    dst_info.texture = texture;
    dst_info.aspect  = GPUTextureAspect::COLOR;

    cmdbuffer.resource_barrier(state_transition(texture, undefined_state(), copy_dst_state()));
    cmdbuffer.copy_buffer_to_texture(src_info, dst_info, {(uint)w, (uint)h, 1});
    cmdbuffer.resource_barrier(state_transition(texture, copy_dst_state(), shader_resource_state(GPUBarrierSync::PIXEL_SHADING)));

    cmdbuffer.submit();
    device.wait();

    staging.destroy();
    stbi_image_free(pixels);

    GUITexture gui_tex = gui->create_texture(texture, texture.create_view());
    return {texture, gui_tex, true};
}

void AssetBrowserView::load_editor_icons()
{
    if (folder_icon.valid && file_icon.valid) return;
    if (!bboard || !bboard->has<GUIRenderer*>()) return;
    auto gui = bboard->get<GUIRenderer*>();

    try {
        auto fs = cmrc::editor::get_filesystem();
        if (!folder_icon.valid && fs.exists("Icons/folder.png")) {
            auto f      = fs.open("Icons/folder.png");
            folder_icon = create_texture_from_memory(f.begin(), f.size(), gui);
        }
        if (!file_icon.valid && fs.exists("Icons/file.png")) {
            auto f    = fs.open("Icons/file.png");
            file_icon = create_texture_from_memory(f.begin(), f.size(), gui);
        }
    } catch (const std::exception& e) {
        spdlog::warn("Failed to load editor icons from CMRC: {}", e.what());
    }
}
