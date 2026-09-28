#include <string>
#include <algorithm>

#include <Lyra/Utilities/Logger.h>
#include <Lyra/Scene/Camera.h>
#include <Lyra/Scene/SceneTree.h>
#include <Lyra/Scene/SceneNode.h>
#include <Lyra/Scene/SceneManager.h>
#include <Lyra/Assets/AMSServer.h>
#include <Lyra/Assets/Format/ModelAsset.h>
#include <Lyra/UISystem/Widgets/UI.h>
#include <Lyra/UISystem/Widgets/UITree.h>
#include <Lyra/UISystem/Widgets/UIDock.h>
#include <Lyra/UISystem/Widgets/UILayout.h>
#include <Lyra/UISystem/Widgets/UIControls.h>
#include <Lyra/UISystem/Widgets/UIDialog.h>
#include <Lyra/UISystem/Widgets/UIIcons.h>

// local imports
#include "HierarchyView.h"

#define LYRA_TREE_VIEW_WINDOW_NAME (LYRA_ICON_TREE " Hierarchy")

using namespace lyra;

HierarchyView::HierarchyView()
{
    // do nothing
}

void HierarchyView::bind(Application& app)
{
    // bind layout manager events
    app.bind<AppEvent::UPDATE, &HierarchyView::update>(*this);
}

static void render_node(
    World& world,
    SceneTree& hierarchy,
    SceneTree::NodeIndex node_idx,
    SceneTree::NodeIndex& selected_node,
    AppContext& context,
    const char* filter,
    Entity& entity_to_delete,
    SceneManager* scene_mgr)
{
    const auto& node   = hierarchy.at(node_idx);
    const auto  entity = node.entity;

    bool is_leaf = (node.first_child == SceneTree::INVALID_NODE);

    // determine node name
    String label_str;
    if (world.any_of<NodeName>(entity)) {
        label_str = world.get_component<NodeName>(entity).name;
    } else {
        label_str = "Node " + std::to_string(static_cast<uint32_t>(entity));
    }
    const char* label = label_str.c_str();

    // filter logic
    bool matches = true;
    if (filter[0] != '\0') {
        String f(filter);
        std::transform(f.begin(), f.end(), f.begin(), ::tolower);
        String n = label_str;
        std::transform(n.begin(), n.end(), n.begin(), ::tolower);
        matches = (n.find(f) != String::npos);
    }

    if (!matches && is_leaf) return;

    // determine icon
    CString icon = "";
    if (world.any_of<Camera>(entity)) {
        icon = LYRA_ICON_CAMERA;
    } else if (!is_leaf) {
        icon = LYRA_ICON_GROUP;
    } else {
        icon = LYRA_ICON_NODE;
    }

    bool is_selected = (selected_node == node_idx);
    auto on_select   = [&]() {
        selected_node                                 = node_idx;
        context.blackboard.get<HierarchyView::Selection>().node = SceneNode(entity);
    };

    auto on_item_context = [&]() {
        ui::item_context_menu([&]() {
            ui::menu_item(LYRA_ICON_NODE " Create Child", [&]() {
                auto child = world.create("ChildNode");
                world.add_child(SceneNode(entity), child);
                if (scene_mgr) scene_mgr->set_dirty(true);
            });
            ui::menu_item(LYRA_ICON_CAMERA " Create Camera", [&]() {
                auto cam = world.create("Camera");
                world.add_component<Camera>(cam);
                world.add_child(SceneNode(entity), cam);
                if (scene_mgr) scene_mgr->set_dirty(true);
            });
            ui::separator();
            ui::menu_item(LYRA_ICON_DELETE " Delete", [&]() {
                entity_to_delete = entity;
            });
        });
    };

    auto handle_drop = [&]() {
        ui::drag_drop_target("LYRA_ASSET_MODEL", [&](const void* data, size_t) {
            CString path_cstr = static_cast<CString>(data);
            auto*   ams       = context.toolboard.try_get<AssetServer*>();
            if (ams && scene_mgr) {
                Path model_path(path_cstr);
                auto model_handle = ams->load_asset<ModelAsset>(path_cstr);
                if (!model_handle.valid()) {
                    model_handle = ams->load_asset<ModelAsset>(model_path.filename().string().c_str());
                }
                bool spawned = false;
                if (model_handle.valid()) {
                    SpawnParams params;
                    params.parent = SceneNode(entity);
                    auto node = scene_mgr->spawn(model_handle, params);
                    if (node.entity != entt::null) {
                        scene_mgr->set_dirty(true);
                        spawned = true;
                    }
                }
                if (!spawned) {
                    ui::dialog::alert("Spawn Failed", "Failed to load or spawn model asset:\n" + model_path.filename().string() + "\n\nSee console log for error details.", ui::StatusRole::Error);
                }
            }
        });
    };

    if (is_leaf) {
        ui::tree_leaf(static_cast<uint64_t>(entity), icon, label, is_selected, on_select);
        handle_drop();
        on_item_context();
    } else {
        auto on_header = [&]() {
            handle_drop();
            on_item_context();
        };
        ui::tree_item(static_cast<uint64_t>(entity), icon, label, is_selected, on_select, on_header, [&]() {
            SceneTree::NodeIndex child_idx = node.first_child;
            while (child_idx != SceneTree::INVALID_NODE) {
                render_node(world, hierarchy, child_idx, selected_node, context, filter, entity_to_delete, scene_mgr);
                child_idx = hierarchy.at(child_idx).next_sibling;
            }
        });
    }
}

void HierarchyView::update(AppContext& context)
{
    lyra::execute_once([&]() {
        ui::workspace::dock(LYRA_TREE_VIEW_WINDOW_NAME, ui::Area::Left);
        context.blackboard.add<Selection>(Selection{});
    });

    auto* scene_mgr = context.toolboard.try_get<SceneManager>();

    ui::panel(LYRA_TREE_VIEW_WINDOW_NAME, [&]() {
        // scene header bar with active scene name and dirty status
        ui::row([&]() {
            String title = (scene_mgr && !scene_mgr->get_active_name().empty())
                ? scene_mgr->get_active_name() : "Untitled";
            if (scene_mgr && scene_mgr->is_dirty()) {
                title += " *";
            }
            ui::label(LYRA_ICON_SCENE);
            ui::label(title.c_str());
            ui::spacer();
            ui::icon_button(LYRA_ICON_NEW_FILE, [&]() {
                if (auto world_ptr = context.toolboard.try_get<World*>()) {
                    auto& world  = *world_ptr;
                    auto  entity = world.create("GameObject");
                    if (scene_mgr) {
                        if (const auto* active = scene_mgr->get_active()) {
                            if (active->root.entity != entt::null) {
                                world.add_child(active->root, SceneNode(entity));
                            }
                        }
                        scene_mgr->set_dirty(true);
                    }
                }
            }, "Add Entity");
        });
        ui::separator();

        ui::search_bar(search_filter, sizeof(search_filter));
        ui::separator();

        Entity entity_to_delete = entt::null;

        if (auto world_ptr = context.toolboard.try_get<World*>()) {
            if (auto hierarchy_ptr = context.toolboard.try_get<SceneTree*>()) {
                auto& world     = *world_ptr;
                auto& hierarchy = *hierarchy_ptr;

                // validate selected_node
                if (selected_node != SceneTree::INVALID_NODE) {
                    if (selected_node >= hierarchy.size() || !world.registry.valid(hierarchy.at(selected_node).entity)) {
                        selected_node = SceneTree::INVALID_NODE;
                        context.blackboard.get<HierarchyView::Selection>().node = SceneNode{};
                    }
                }

                const auto* active                  = scene_mgr ? scene_mgr->get_active() : nullptr;
                bool        rendered_scene_children = false;

                if (active && active->root.entity != entt::null && search_filter[0] == '\0') {
                    auto root_tree_idx = hierarchy.find_node(active->root.entity);
                    if (root_tree_idx != SceneTree::INVALID_NODE) {
                        SceneTree::NodeIndex child_idx = hierarchy[root_tree_idx].first_child;
                        while (child_idx != SceneTree::INVALID_NODE) {
                            render_node(world, hierarchy, child_idx, selected_node, context, search_filter, entity_to_delete, scene_mgr);
                            child_idx = hierarchy[child_idx].next_sibling;
                        }
                        rendered_scene_children = true;
                    }
                }

                if (!rendered_scene_children) {
                    for (auto root_idx : hierarchy) {
                        render_node(world, hierarchy, root_idx, selected_node, context, search_filter, entity_to_delete, scene_mgr);
                    }
                }

                Vector2 avail = ui::get_available_space();
                if (avail.x > 10.0f && avail.y > 10.0f) {
                    ui::invisible_button("##hierarchy_empty_space", avail);
                    ui::drag_drop_target("LYRA_ASSET_MODEL", [&](const void* data, size_t) {
                        CString path_cstr = static_cast<CString>(data);
                        auto*   ams       = context.toolboard.try_get<AssetServer*>();
                        if (ams && scene_mgr) {
                            Path model_path(path_cstr);
                            auto model_handle = ams->load_asset<ModelAsset>(path_cstr);
                            if (!model_handle.valid()) {
                                model_handle = ams->load_asset<ModelAsset>(model_path.filename().string().c_str());
                            }
                            bool spawned = false;
                            if (model_handle.valid()) {
                                auto node = scene_mgr->spawn(model_handle);
                                if (node.entity != entt::null) {
                                    scene_mgr->set_dirty(true);
                                    spawned = true;
                                }
                            }
                            if (!spawned) {
                                ui::dialog::alert("Spawn Failed", "Failed to load or spawn model asset:\n" + model_path.filename().string() + "\n\nSee console log for error details.", ui::StatusRole::Error);
                            }
                        }
                    });
                }
            }
        }

        // background context menu
        ui::panel_context_menu([&]() {
            ui::menu_item(LYRA_ICON_NODE " Create Empty Entity", [&]() {
                if (auto world_ptr = context.toolboard.try_get<World*>()) {
                    auto& world  = *world_ptr;
                    auto  entity = world.create("GameObject");
                    if (scene_mgr) {
                        if (const auto* active = scene_mgr->get_active()) {
                            if (active->root.entity != entt::null) {
                                world.add_child(active->root, SceneNode(entity));
                            }
                        }
                        scene_mgr->set_dirty(true);
                    }
                }
            });
            ui::menu_item(LYRA_ICON_CAMERA " Create Camera", [&]() {
                if (auto world_ptr = context.toolboard.try_get<World*>()) {
                    auto& world = *world_ptr;
                    auto  cam   = world.create("Camera");
                    world.add_component<Camera>(cam);
                    if (scene_mgr) {
                        if (const auto* active = scene_mgr->get_active()) {
                            if (active->root.entity != entt::null) {
                                world.add_child(active->root, SceneNode(cam));
                            }
                        }
                        scene_mgr->set_dirty(true);
                    }
                }
            });
        });

        // process deferred deletion
        if (entity_to_delete != entt::null) {
            if (auto world_ptr = context.toolboard.try_get<World*>()) {
                auto& world = *world_ptr;
                world.destroy_tree(SceneNode(entity_to_delete));
                selected_node = SceneTree::INVALID_NODE;
                context.blackboard.get<HierarchyView::Selection>().node = SceneNode{};
                if (scene_mgr) scene_mgr->set_dirty(true);
            }
            entity_to_delete = entt::null;
        }
    });
}
