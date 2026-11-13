#include "EditorCommands.h"

#include <Lyra/Lyra.hpp>
#include <Lyra/UISystem/Widgets/UICommand.h>
#include <Lyra/UISystem/Widgets/UI.h>
#include <Lyra/UISystem/Widgets/UIDock.h>
#include <Lyra/UISystem/Widgets/UIIcons.h>
#include <Lyra/UISystem/Widgets/UIDialog.h>

#include "Panels/AssetBrowserView.h"
#include "Panels/HierarchyView.h"

using namespace lyra;

namespace
{
    bool s_show_new_scene_modal = false;
    bool s_open_new_scene_modal = false;
    char s_new_scene_name[128]  = "Untitled";
} // namespace

void lyra::setup_editor_commands(AppContext& context)
{
    auto reset_input_states = [&context]() {
        if (auto input = context.toolboard.try_get<InputManager*>()) {
            input->reset();
        }
        if (auto window = context.toolboard.try_get<Window*>()) {
            window->reset_input_state();
        }
        ui::clear_input_keys();
    };

    auto to_app_vpath = [&context](const Path& os_path) -> Optional<String> {
        if (auto* browser = context.toolboard.try_get<AssetBrowserView>()) {
            std::error_code ec;
            Path            rel = fs::relative(os_path, browser->get_app_root(), ec);
            if (!ec && !rel.empty() && *rel.begin() != "..") {
                return VFSPath(vfs::SchemeApp, rel.generic_string()).to_string();
            }
        }
        return std::nullopt;
    };

    // 1. scene commands
    ui::command::register_command({
        .id       = "scene.new",
        .category = "Scene",
        .title    = "New Scene",
        .icon     = LYRA_ICON_SCENE,
        .shortcut = "Ctrl+N",
        .chord    = ButtonChord::key(KeyButton::N, ModifierKey::CTRL),
        .action   = [&context, reset_input_states]() {
            auto* scene_mgr = context.toolboard.try_get<SceneManager>();
            if (scene_mgr && scene_mgr->is_dirty()) {
                if (!ui::dialog::confirm("Unsaved Changes", "The active scene has unsaved changes. Discard and create a new scene?")) {
                    return;
                }
            }
            strcpy_s(s_new_scene_name, sizeof(s_new_scene_name), "Untitled");
            s_show_new_scene_modal = true;
            s_open_new_scene_modal = true;
            reset_input_states();
        },
        .is_enabled = [&context]() { return context.toolboard.try_get<SceneManager>() != nullptr; },
    });

    ui::command::register_command({
        .id       = "scene.open",
        .category = "Scene",
        .title    = "Open Scene...",
        .icon     = LYRA_ICON_FOLDER,
        .shortcut = "Ctrl+O",
        .chord    = ButtonChord::key(KeyButton::O, ModifierKey::CTRL),
        .action   = [&context, reset_input_states, to_app_vpath]() {
            auto* scene_mgr = context.toolboard.try_get<SceneManager>();
            if (!scene_mgr) return;
            if (scene_mgr->is_dirty()) {
                if (!ui::dialog::confirm("Unsaved Changes", "The active scene has unsaved changes. Discard and open scene?")) {
                    return;
                }
            }
            ui::dialog::Options opts;
            opts.title    = "Open Scene";
            opts.filters  = {{"Lyra Scene (*.scene)", "*.scene"}};
            auto selected = ui::dialog::open_file(opts);
            reset_input_states();
            if (selected.has_value()) {
                auto vpath = to_app_vpath(*selected);
                if (!vpath) {
                    ui::dialog::alert("Load Failed", "Scenes must reside inside the project assets folder:\n" + selected->string(), ui::StatusRole::Error);
                    return;
                }
                auto res = scene_mgr->load(Path(*vpath), LoadMode::SINGLE);
                if (res == INVALID_SCENE_INSTANCE) {
                    ui::dialog::alert("Load Failed", "Failed to load scene:\n" + *vpath + "\n\nSee console log for error details.", ui::StatusRole::Error);
                }
            }
        },
        .is_enabled = [&context]() { return context.toolboard.try_get<SceneManager>() != nullptr; },
    });

    ui::command::register_command({
        .id       = "scene.save",
        .category = "Scene",
        .title    = "Save Scene",
        .icon     = LYRA_ICON_SAVE,
        .shortcut = "Ctrl+S",
        .chord    = ButtonChord::key(KeyButton::S, ModifierKey::CTRL),
        .action   = [&context, reset_input_states, to_app_vpath]() {
            auto* scene_mgr = context.toolboard.try_get<SceneManager>();
            if (!scene_mgr) return;
            if (!scene_mgr->get_active_path().empty()) {
                if (!scene_mgr->save_active()) {
                    ui::dialog::alert("Save Failed", "Failed to save active scene:\n" + scene_mgr->get_active_path().string(), ui::StatusRole::Error);
                }
                reset_input_states();
            } else {
                ui::command::execute("scene.save_as");
            }
        },
        .is_enabled = [&context]() { return context.toolboard.try_get<SceneManager>() != nullptr; },
    });

    ui::command::register_command({
        .id       = "scene.save_as",
        .category = "Scene",
        .title    = "Save Scene As...",
        .icon     = LYRA_ICON_SAVE,
        .shortcut = "Ctrl+Shift+S",
        .chord    = ButtonChord::key(KeyButton::S, ModifierKey::CTRL | ModifierKey::SHIFT),
        .action   = [&context, reset_input_states, to_app_vpath]() {
            auto* scene_mgr = context.toolboard.try_get<SceneManager>();
            if (!scene_mgr) return;
            ui::dialog::Options opts;
            opts.title    = "Save Scene As";
            opts.filters  = {{"Lyra Scene (*.scene)", "*.scene"}};
            auto selected = ui::dialog::save_file(opts);
            reset_input_states();
            if (selected.has_value()) {
                Path p = *selected;
                if (p.extension() != ".scene") {
                    p += ".scene";
                }
                auto vpath = to_app_vpath(p);
                if (!vpath) {
                    ui::dialog::alert("Save Failed", "Scenes must be saved inside the project assets folder:\n" + p.string(), ui::StatusRole::Error);
                    return;
                }
                if (!scene_mgr->save(Path(*vpath))) {
                    ui::dialog::alert("Save Failed", "Failed to save scene to:\n" + *vpath, ui::StatusRole::Error);
                }
            }
        },
        .is_enabled = [&context]() { return context.toolboard.try_get<SceneManager>() != nullptr; },
    });

    ui::command::register_command({
        .id       = "scene.load_additive",
        .category = "Scene",
        .title    = "Load Scene Additive...",
        .icon     = LYRA_ICON_IMPORT,
        .action   = [&context, reset_input_states, to_app_vpath]() {
            auto* scene_mgr = context.toolboard.try_get<SceneManager>();
            if (!scene_mgr) return;
            ui::dialog::Options opts;
            opts.title    = "Load Scene Additive";
            opts.filters  = {{"Lyra Scene (*.scene)", "*.scene"}};
            auto selected = ui::dialog::open_file(opts);
            reset_input_states();
            if (selected.has_value()) {
                auto vpath = to_app_vpath(*selected);
                if (!vpath) {
                    ui::dialog::alert("Load Failed", "Scenes must reside inside the project assets folder:\n" + selected->string(), ui::StatusRole::Error);
                    return;
                }
                auto res = scene_mgr->load(Path(*vpath), LoadMode::ADDITIVE);
                if (res == INVALID_SCENE_INSTANCE) {
                    ui::dialog::alert("Load Failed", "Failed to load scene additively:\n" + *vpath + "\n\nSee console log for error details.", ui::StatusRole::Error);
                }
            }
        },
        .is_enabled = [&context]() { return context.toolboard.try_get<SceneManager>() != nullptr; },
    });

    // 2. assets commands
    ui::command::register_command({
        .id       = "assets.reimport_all",
        .category = "Assets",
        .title    = "Reimport All (Force)",
        .icon     = LYRA_ICON_RELOAD,
        .action   = [&context]() {
            if (auto* ams = context.toolboard.try_get<AssetServer>()) {
                ams->reimport_all(true);
            }
        },
        .is_enabled = [&context]() { return context.toolboard.try_get<AssetServer>() != nullptr; },
    });

    ui::command::register_command({
        .id       = "assets.purge",
        .category = "Assets",
        .title    = "Purge Unused Assets",
        .icon     = LYRA_ICON_DELETE,
        .action   = [&context]() {
            if (auto* ams = context.toolboard.try_get<AssetServer>()) {
                ams->purge();
            }
        },
        .is_enabled = [&context]() { return context.toolboard.try_get<AssetServer>() != nullptr; },
    });

    ui::command::register_command({
        .id       = "assets.flush",
        .category = "Assets",
        .title    = "Flush Registry to Disk",
        .icon     = LYRA_ICON_SAVE,
        .action   = [&context]() {
            if (auto* ams = context.toolboard.try_get<AssetServer>()) {
                ams->flush();
            }
        },
        .is_enabled = [&context]() { return context.toolboard.try_get<AssetServer>() != nullptr; },
    });

    ui::command::register_command({
        .id       = "assets.toggle_watching",
        .category = "Assets",
        .title    = "Toggle Watch Source Directory",
        .icon     = LYRA_ICON_SETTINGS,
        .action   = [&context]() {
            if (auto* ams = context.toolboard.try_get<AssetServer>()) {
                ams->set_watching(!ams->is_watching());
            }
        },
        .is_enabled = [&context]() { return context.toolboard.try_get<AssetServer>() != nullptr; },
    });

    // 3. window & layout commands
    ui::command::register_command({
        .id       = "window.command_palette",
        .category = "Window",
        .title    = "Command Palette",
        .icon     = LYRA_ICON_SEARCH,
        .shortcut = "Ctrl+P",
        .chord    = ButtonChord::key(KeyButton::P, ModifierKey::CTRL),
        .action   = []() {
            ui::command::toggle_palette();
        },
    });

    ui::command::register_command({
        .id       = "window.reset_layout",
        .category = "Window",
        .title    = "Reset Layout",
        .icon     = LYRA_ICON_TREE,
        .action   = []() {
            ui::workspace::reset_layout();
        },
    });

    // 4. console commands
    ui::command::register_command({
        .id       = "console.clear",
        .category = "Console",
        .title    = "Clear Console Logs",
        .icon     = LYRA_ICON_DELETE,
        .action   = []() {
            get_console_sink()->get_console().clear();
        },
    });

    // 5. entity creation commands
    ui::command::register_command({
        .id       = "entity.create_empty",
        .category = "Entity",
        .title    = "Create Empty Child",
        .icon     = LYRA_ICON_NODE,
        .action   = [&context]() {
            auto* scene_mgr = context.toolboard.try_get<SceneManager>();
            auto* world     = context.toolboard.try_get<World*>();
            if (!world) return;
            SceneNode parent = context.blackboard.has<HierarchyView::Selection>()
                                   ? context.blackboard.get<HierarchyView::Selection>().node
                                   : SceneNode{};
            String name  = scene_mgr ? scene_mgr->resolve_unique_name(parent, "ChildNode") : "ChildNode";
            auto   child = world->create(name);
            if (parent.entity != entt::null) {
                world->add_child(parent, child);
            }
            if (scene_mgr) scene_mgr->set_dirty(true);
        },
    });

    ui::command::register_command({
        .id       = "entity.create_camera",
        .category = "Entity",
        .title    = "Create Camera",
        .icon     = LYRA_ICON_CAMERA,
        .action   = [&context]() {
            auto* scene_mgr = context.toolboard.try_get<SceneManager>();
            auto* world     = context.toolboard.try_get<World*>();
            if (!world) return;
            SceneNode parent = context.blackboard.has<HierarchyView::Selection>()
                                   ? context.blackboard.get<HierarchyView::Selection>().node
                                   : SceneNode{};
            String name  = scene_mgr ? scene_mgr->resolve_unique_name(parent, "Camera") : "Camera";
            auto   cam   = world->create(name);
            world->add_component<Camera>(cam);
            if (parent.entity != entt::null) {
                world->add_child(parent, cam);
            }
            if (scene_mgr) scene_mgr->set_dirty(true);
        },
    });

    ui::command::register_command({
        .id       = "entity.create_dir_light",
        .category = "Entity",
        .title    = "Create Directional Light",
        .icon     = LYRA_ICON_SUN,
        .action   = [&context]() {
            auto* scene_mgr = context.toolboard.try_get<SceneManager>();
            auto* world     = context.toolboard.try_get<World*>();
            if (!world) return;
            SceneNode parent = context.blackboard.has<HierarchyView::Selection>()
                                   ? context.blackboard.get<HierarchyView::Selection>().node
                                   : SceneNode{};
            String name  = scene_mgr ? scene_mgr->resolve_unique_name(parent, "DirectionalLight") : "DirectionalLight";
            auto   light = world->create(name);
            world->add_component<DirectionalLight>(light);
            if (parent.entity != entt::null) {
                world->add_child(parent, light);
            }
            if (scene_mgr) scene_mgr->set_dirty(true);
        },
    });

    ui::command::register_command({
        .id       = "entity.create_point_light",
        .category = "Entity",
        .title    = "Create Point Light",
        .icon     = LYRA_ICON_LIGHT,
        .action   = [&context]() {
            auto* scene_mgr = context.toolboard.try_get<SceneManager>();
            auto* world     = context.toolboard.try_get<World*>();
            if (!world) return;
            SceneNode parent = context.blackboard.has<HierarchyView::Selection>()
                                   ? context.blackboard.get<HierarchyView::Selection>().node
                                   : SceneNode{};
            String name  = scene_mgr ? scene_mgr->resolve_unique_name(parent, "PointLight") : "PointLight";
            auto   light = world->create(name);
            world->add_component<PointLight>(light);
            if (parent.entity != entt::null) {
                world->add_child(parent, light);
            }
            if (scene_mgr) scene_mgr->set_dirty(true);
        },
    });
}

void lyra::render_editor_modals(AppContext& context)
{
    if (s_open_new_scene_modal) {
        ui::open_modal(LYRA_ICON_SCENE " New Scene");
        s_open_new_scene_modal = false;
    }

    if (s_show_new_scene_modal) {
        auto* scene_mgr = context.toolboard.try_get<SceneManager>();

        ui::modal(LYRA_ICON_SCENE " New Scene", &s_show_new_scene_modal, [&]() {
            ui::label("Enter scene name:");
            ui::text_field("##new_scene_name", s_new_scene_name, sizeof(s_new_scene_name), [&]() {
                if (s_new_scene_name[0] != '\0' && scene_mgr) {
                    scene_mgr->create(s_new_scene_name);
                }
                s_show_new_scene_modal = false;
                ui::close_modal();
            });

            ui::separator();

            ui::row(ui::Alignment::End, [&]() {
                ui::button("Cancel", [&]() {
                    s_show_new_scene_modal = false;
                    ui::close_modal();
                });

                ui::button("Create", [&]() {
                    if (s_new_scene_name[0] != '\0' && scene_mgr) {
                        scene_mgr->create(s_new_scene_name);
                    }
                    s_show_new_scene_modal = false;
                    ui::close_modal();
                },
                    ui::ButtonRole::Primary);
            });
        });
    }
}
