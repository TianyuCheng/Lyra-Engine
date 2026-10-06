#include <string>
#include <cstring>
#include <algorithm>

#include <Lyra/Utilities/Logger.h>
#include <Lyra/Scene/Camera.h>
#include <Lyra/Scene/Light.h>
#include <Lyra/Scene/Mesh.h>
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

struct NodeRenderContext
{
    World&                world;
    SceneTree&            hierarchy;
    SceneTree::NodeIndex& selected_node;
    AppContext&           context;
    const char*           filter;
    Entity&               entity_to_delete;
    Entity&               entity_to_duplicate;
    Entity&               entity_to_rename;
    char*                 rename_buffer;
    size_t                rename_buffer_size;
    bool&                 open_rename_modal;
    bool&                 show_rename_modal;
    Entity&               entity_to_delete_children;
    SceneManager*         scene_mgr;
};

static Entity duplicate_entity(World& world, Entity src, Entity new_parent, SceneManager* scene_mgr)
{
    String src_name = "Node";
    if (world.any_of<NodeName>(src)) {
        src_name = world.get_component<NodeName>(src).name;
    }

    SceneNode target_parent = (new_parent != entt::null) ? SceneNode(new_parent) : SceneNode{};
    if (target_parent.entity == entt::null) {
        if (auto* p = world.registry.try_get<Parent>(src)) {
            target_parent = p->node;
        } else if (scene_mgr) {
            if (const auto* active = scene_mgr->get_active()) {
                if (active->root.entity != entt::null && active->root.entity != src) {
                    target_parent = active->root;
                }
            }
        }
    }
    String new_name = scene_mgr ? scene_mgr->resolve_unique_name(target_parent, src_name) : (src_name + " (Copy)");

    Entity clone = world.create(new_name);

    if (auto* t = world.registry.try_get<TransformLocal>(src)) {
        world.registry.replace<TransformLocal>(clone, *t);
    }
    if (auto* c = world.registry.try_get<Camera>(src)) {
        world.registry.emplace_or_replace<Camera>(clone, *c);
    }
    if (auto* pl = world.registry.try_get<PointLight>(src)) {
        world.registry.emplace_or_replace<PointLight>(clone, *pl);
    }
    if (auto* sl = world.registry.try_get<SpotLight>(src)) {
        world.registry.emplace_or_replace<SpotLight>(clone, *sl);
    }
    if (auto* dl = world.registry.try_get<DirectionalLight>(src)) {
        world.registry.emplace_or_replace<DirectionalLight>(clone, *dl);
    }
    if (auto* m = world.registry.try_get<Mesh>(src)) {
        world.registry.emplace_or_replace<Mesh>(clone, *m);
    }

    if (new_parent != entt::null) {
        world.add_child(SceneNode(new_parent), SceneNode(clone));
    } else {
        if (auto* p = world.registry.try_get<Parent>(src)) {
            world.add_child(p->node, SceneNode(clone));
        } else if (scene_mgr) {
            if (const auto* active = scene_mgr->get_active()) {
                if (active->root.entity != entt::null && active->root.entity != src) {
                    world.add_child(active->root, SceneNode(clone));
                }
            }
        }
    }

    if (auto* children = world.registry.try_get<Children>(src)) {
        auto child_nodes = children->nodes;
        for (auto child_node : child_nodes) {
            duplicate_entity(world, child_node.entity, clone, scene_mgr);
        }
    }

    return clone;
}

static void render_node(
    const NodeRenderContext& ctx,
    SceneTree::NodeIndex     node_idx)
{
    auto&       world     = ctx.world;
    auto&       hierarchy = ctx.hierarchy;
    const auto& node      = hierarchy.at(node_idx);
    const auto  entity    = node.entity;

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
    if (ctx.filter[0] != '\0') {
        String f(ctx.filter);
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
    } else if (world.any_of<DirectionalLight>(entity)) {
        icon = LYRA_ICON_SUN;
    } else if (world.any_of<PointLight, SpotLight>(entity)) {
        icon = LYRA_ICON_LIGHT;
    } else if (!is_leaf) {
        icon = LYRA_ICON_GROUP;
    } else {
        icon = LYRA_ICON_NODE;
    }

    bool is_selected = (ctx.selected_node == node_idx);
    auto on_select   = [&]() {
        ctx.selected_node                                           = node_idx;
        ctx.context.blackboard.get<HierarchyView::Selection>().node = SceneNode(entity);
    };

    auto on_item_context = [&]() {
        ui::item_context_menu([&]() {
            ui::menu(LYRA_ICON_NODE " Create", [&]() {
                ui::menu_item(LYRA_ICON_NODE " Empty Child", [&]() {
                    String name  = ctx.scene_mgr ? ctx.scene_mgr->resolve_unique_name(SceneNode(entity), "ChildNode") : "ChildNode";
                    auto   child = world.create(name);
                    world.add_child(SceneNode(entity), child);
                    if (ctx.scene_mgr) ctx.scene_mgr->set_dirty(true);
                });
                ui::menu_item(LYRA_ICON_CAMERA " Camera", [&]() {
                    String name = ctx.scene_mgr ? ctx.scene_mgr->resolve_unique_name(SceneNode(entity), "Camera") : "Camera";
                    auto   cam  = world.create(name);
                    world.add_component<Camera>(cam);
                    world.add_child(SceneNode(entity), cam);
                    if (ctx.scene_mgr) ctx.scene_mgr->set_dirty(true);
                });
                ui::menu(LYRA_ICON_LIGHT " Light", [&]() {
                    ui::menu_item(LYRA_ICON_SUN " Directional Light", [&]() {
                        String name  = ctx.scene_mgr ? ctx.scene_mgr->resolve_unique_name(SceneNode(entity), "DirectionalLight") : "DirectionalLight";
                        auto   light = world.create(name);
                        world.add_component<DirectionalLight>(light);
                        world.add_child(SceneNode(entity), light);
                        if (ctx.scene_mgr) ctx.scene_mgr->set_dirty(true);
                    });
                    ui::menu_item(LYRA_ICON_LIGHT " Point Light", [&]() {
                        String name  = ctx.scene_mgr ? ctx.scene_mgr->resolve_unique_name(SceneNode(entity), "PointLight") : "PointLight";
                        auto   light = world.create(name);
                        world.add_component<PointLight>(light);
                        world.add_child(SceneNode(entity), light);
                        if (ctx.scene_mgr) ctx.scene_mgr->set_dirty(true);
                    });
                    ui::menu_item(LYRA_ICON_LIGHT " Spot Light", [&]() {
                        String name  = ctx.scene_mgr ? ctx.scene_mgr->resolve_unique_name(SceneNode(entity), "SpotLight") : "SpotLight";
                        auto   light = world.create(name);
                        world.add_component<SpotLight>(light);
                        world.add_child(SceneNode(entity), light);
                        if (ctx.scene_mgr) ctx.scene_mgr->set_dirty(true);
                    });
                });
            });

            ui::separator();

            ui::menu_item(LYRA_ICON_NEW_FILE " Duplicate", "Ctrl+D", [&]() {
                ctx.entity_to_duplicate = entity;
            });

            ui::menu_item(LYRA_ICON_RENAME " Rename", "F2", [&]() {
                ctx.entity_to_rename = entity;
                if (world.any_of<NodeName>(entity)) {
                    auto& cur_name = world.get_component<NodeName>(entity).name;
                    std::strncpy(ctx.rename_buffer, cur_name.c_str(), ctx.rename_buffer_size - 1);
                    ctx.rename_buffer[ctx.rename_buffer_size - 1] = '\0';
                } else {
                    ctx.rename_buffer[0] = '\0';
                }
                ctx.open_rename_modal = true;
                ctx.show_rename_modal = true;
            });

            ui::separator();

            bool has_parent = world.any_of<Parent>(entity);
            if (has_parent) {
                ui::menu_item(LYRA_ICON_TREE " Unparent to Root", [&]() {
                    if (auto* old_p = world.registry.try_get<Parent>(entity)) {
                        world.del_child(old_p->node, SceneNode(entity));
                    }
                    if (ctx.scene_mgr) {
                        if (const auto* active = ctx.scene_mgr->get_active()) {
                            if (active->root.entity != entt::null && active->root.entity != entity) {
                                world.add_child(active->root, SceneNode(entity));
                            }
                        }
                        ctx.scene_mgr->set_dirty(true);
                    }
                });
            }

            bool has_children = world.any_of<Children>(entity) && !world.get_component<Children>(entity).nodes.empty();
            if (has_children) {
                ui::menu_item(LYRA_ICON_DELETE " Delete Children", [&]() {
                    ctx.entity_to_delete_children = entity;
                });
            }

            ui::separator();

            ui::menu_item(LYRA_ICON_DELETE " Delete", "Del", [&]() {
                ctx.entity_to_delete = entity;
            });
        });
    };

    auto handle_drop = [&]() {
        ui::drag_drop_target("LYRA_ASSET_MODEL", [&](const void* data, size_t) {
            CString path_cstr = static_cast<CString>(data);
            auto*   ams       = ctx.context.toolboard.try_get<AssetServer*>();
            if (ams && ctx.scene_mgr) {
                Path model_path(path_cstr);
                auto model_handle = ams->load_asset<ModelAsset>(path_cstr);
                if (!model_handle.valid()) {
                    model_handle = ams->load_asset<ModelAsset>(model_path.filename().generic_string().c_str());
                }
                if (!model_handle.valid() && ams->has_cooker_for(model_path)) {
                    auto [full_path, rel_path] = ams->resolve_asset_path(model_path);
                    auto    fut                = ams->import_asset(rel_path, false);
                    AssetID cooked_guid        = fut.get();
                    if (cooked_guid != 0) {
                        model_handle = ams->load_asset<ModelAsset>(cooked_guid);
                    }
                }
                bool spawned = false;
                if (model_handle.valid()) {
                    SpawnParams params;
                    params.parent = SceneNode(entity);
                    params.name   = model_path.stem().string();
                    auto node     = ctx.scene_mgr->spawn(model_handle, params);
                    if (node.entity != entt::null) {
                        ctx.scene_mgr->set_dirty(true);
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
                render_node(ctx, child_idx);
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
                               ? scene_mgr->get_active_name()
                               : "Untitled";
            if (scene_mgr && scene_mgr->is_dirty()) {
                title += " *";
            }
            ui::label(LYRA_ICON_SCENE);
            ui::label(title.c_str());
            ui::spacer();
            ui::icon_button(LYRA_ICON_NEW_FILE, [&]() {
                if (auto world_ptr = context.toolboard.try_get<World*>()) {
                    auto&     world = *world_ptr;
                    SceneNode target_parent{};
                    if (scene_mgr) {
                        if (const auto* active = scene_mgr->get_active()) {
                            target_parent = active->root;
                        }
                    }
                    String name   = scene_mgr ? scene_mgr->resolve_unique_name(target_parent, "GameObject") : "GameObject";
                    auto   entity = world.create(name);
                    if (scene_mgr) {
                        if (target_parent.entity != entt::null) {
                            world.add_child(target_parent, SceneNode(entity));
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
                        selected_node                                           = SceneTree::INVALID_NODE;
                        context.blackboard.get<HierarchyView::Selection>().node = SceneNode{};
                    }
                }

                NodeRenderContext render_ctx{
                    world,
                    hierarchy,
                    selected_node,
                    context,
                    search_filter,
                    entity_to_delete,
                    entity_to_duplicate,
                    entity_to_rename,
                    rename_buffer,
                    sizeof(rename_buffer),
                    open_rename_modal,
                    show_rename_modal,
                    entity_to_delete_children,
                    scene_mgr};

                const auto* active                  = scene_mgr ? scene_mgr->get_active() : nullptr;
                bool        rendered_scene_children = false;

                if (active && active->root.entity != entt::null && search_filter[0] == '\0') {
                    auto root_tree_idx = hierarchy.find_node(active->root.entity);
                    if (root_tree_idx != SceneTree::INVALID_NODE) {
                        SceneTree::NodeIndex child_idx = hierarchy[root_tree_idx].first_child;
                        while (child_idx != SceneTree::INVALID_NODE) {
                            render_node(render_ctx, child_idx);
                            child_idx = hierarchy[child_idx].next_sibling;
                        }
                        rendered_scene_children = true;
                    }
                }

                if (!rendered_scene_children) {
                    for (auto root_idx : hierarchy) {
                        render_node(render_ctx, root_idx);
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
                                model_handle = ams->load_asset<ModelAsset>(model_path.filename().generic_string().c_str());
                            }
                            if (!model_handle.valid() && ams->has_cooker_for(model_path)) {
                                auto [full_path, rel_path] = ams->resolve_asset_path(model_path);
                                auto    fut                = ams->import_asset(rel_path, false);
                                AssetID cooked_guid        = fut.get();
                                if (cooked_guid != 0) {
                                    model_handle = ams->load_asset<ModelAsset>(cooked_guid);
                                }
                            }
                            bool spawned = false;
                            if (model_handle.valid()) {
                                SpawnParams params;
                                params.name = model_path.stem().string();
                                auto node   = scene_mgr->spawn(model_handle, params);
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
            ui::menu(LYRA_ICON_NODE " Create", [&]() {
                auto get_active_root = [&]() -> SceneNode {
                    if (scene_mgr) {
                        if (const auto* active = scene_mgr->get_active()) {
                            return active->root;
                        }
                    }
                    return SceneNode{};
                };
                ui::menu_item(LYRA_ICON_NODE " Empty Entity", [&]() {
                    if (auto world_ptr = context.toolboard.try_get<World*>()) {
                        auto&     world  = *world_ptr;
                        SceneNode root   = get_active_root();
                        String    name   = scene_mgr ? scene_mgr->resolve_unique_name(root, "GameObject") : "GameObject";
                        auto      entity = world.create(name);
                        if (root.entity != entt::null) {
                            world.add_child(root, SceneNode(entity));
                        }
                        if (scene_mgr) scene_mgr->set_dirty(true);
                    }
                });
                ui::menu_item(LYRA_ICON_CAMERA " Camera", [&]() {
                    if (auto world_ptr = context.toolboard.try_get<World*>()) {
                        auto&     world = *world_ptr;
                        SceneNode root  = get_active_root();
                        String    name  = scene_mgr ? scene_mgr->resolve_unique_name(root, "Camera") : "Camera";
                        auto      cam   = world.create(name);
                        world.add_component<Camera>(cam);
                        if (root.entity != entt::null) {
                            world.add_child(root, SceneNode(cam));
                        }
                        if (scene_mgr) scene_mgr->set_dirty(true);
                    }
                });
                ui::menu(LYRA_ICON_LIGHT " Light", [&]() {
                    ui::menu_item(LYRA_ICON_SUN " Directional Light", [&]() {
                        if (auto world_ptr = context.toolboard.try_get<World*>()) {
                            auto&     world = *world_ptr;
                            SceneNode root  = get_active_root();
                            String    name  = scene_mgr ? scene_mgr->resolve_unique_name(root, "DirectionalLight") : "DirectionalLight";
                            auto      light = world.create(name);
                            world.add_component<DirectionalLight>(light);
                            if (root.entity != entt::null) {
                                world.add_child(root, SceneNode(light));
                            }
                            if (scene_mgr) scene_mgr->set_dirty(true);
                        }
                    });
                    ui::menu_item(LYRA_ICON_LIGHT " Point Light", [&]() {
                        if (auto world_ptr = context.toolboard.try_get<World*>()) {
                            auto&     world = *world_ptr;
                            SceneNode root  = get_active_root();
                            String    name  = scene_mgr ? scene_mgr->resolve_unique_name(root, "PointLight") : "PointLight";
                            auto      light = world.create(name);
                            world.add_component<PointLight>(light);
                            if (root.entity != entt::null) {
                                world.add_child(root, SceneNode(light));
                            }
                            if (scene_mgr) scene_mgr->set_dirty(true);
                        }
                    });
                    ui::menu_item(LYRA_ICON_LIGHT " Spot Light", [&]() {
                        if (auto world_ptr = context.toolboard.try_get<World*>()) {
                            auto&     world = *world_ptr;
                            SceneNode root  = get_active_root();
                            String    name  = scene_mgr ? scene_mgr->resolve_unique_name(root, "SpotLight") : "SpotLight";
                            auto      light = world.create(name);
                            world.add_component<SpotLight>(light);
                            if (root.entity != entt::null) {
                                world.add_child(root, SceneNode(light));
                            }
                            if (scene_mgr) scene_mgr->set_dirty(true);
                        }
                    });
                });
            });

            ui::separator();

            ui::menu_item(LYRA_ICON_TREE " Deselect", [&]() {
                selected_node                                           = SceneTree::INVALID_NODE;
                context.blackboard.get<HierarchyView::Selection>().node = SceneNode{};
            });

            ui::separator();

            ui::menu_item(LYRA_ICON_DELETE " Clear Scene", [&]() {
                if (ui::dialog::confirm("Clear Scene", "Are you sure you want to delete all entities in the active scene?")) {
                    if (auto world_ptr = context.toolboard.try_get<World*>()) {
                        auto& world = *world_ptr;
                        if (scene_mgr) {
                            if (const auto* active = scene_mgr->get_active()) {
                                if (active->root.entity != entt::null) {
                                    if (auto* children = world.registry.try_get<Children>(active->root.entity)) {
                                        auto nodes = children->nodes;
                                        for (auto child : nodes) {
                                            world.destroy_tree(child);
                                        }
                                        world.registry.remove<Children>(active->root.entity);
                                    }
                                }
                            }
                            scene_mgr->set_dirty(true);
                        }
                        selected_node                                           = SceneTree::INVALID_NODE;
                        context.blackboard.get<HierarchyView::Selection>().node = SceneNode{};
                    }
                }
            });
        });

        // keyboard shortcuts for selected entity
        if (!ui::is_text_input_active() && ui::is_panel_hovered()) {
            if (selected_node != SceneTree::INVALID_NODE) {
                if (auto world_ptr = context.toolboard.try_get<World*>()) {
                    if (auto hierarchy_ptr = context.toolboard.try_get<SceneTree*>()) {
                        Entity sel_entity = (*hierarchy_ptr)[selected_node].entity;
                        if (sel_entity != entt::null && world_ptr->registry.valid(sel_entity)) {
                            if (ui::is_key_pressed(KeyButton::F2)) {
                                entity_to_rename = sel_entity;
                                if (world_ptr->any_of<NodeName>(sel_entity)) {
                                    auto& cur_name = world_ptr->get_component<NodeName>(sel_entity).name;
                                    std::strncpy(rename_buffer, cur_name.c_str(), sizeof(rename_buffer) - 1);
                                    rename_buffer[sizeof(rename_buffer) - 1] = '\0';
                                } else {
                                    rename_buffer[0] = '\0';
                                }
                                open_rename_modal = true;
                                show_rename_modal = true;
                            } else if (ui::is_key_down(KeyButton::CTRL) && ui::is_key_pressed(KeyButton::D)) {
                                entity_to_duplicate = sel_entity;
                            } else if (ui::is_key_pressed(KeyButton::DEL)) {
                                entity_to_delete = sel_entity;
                            }
                        }
                    }
                }
            }
        }

        // rename modal dialog
        if (open_rename_modal) {
            ui::open_modal(LYRA_ICON_RENAME " Rename Node");
            open_rename_modal = false;
        }

        if (show_rename_modal) {
            ui::modal(LYRA_ICON_RENAME " Rename Node", &show_rename_modal, [&]() {
                ui::label("New name:");
                ui::text_field("##rename_node_field", rename_buffer, sizeof(rename_buffer), [&]() {
                    if (entity_to_rename != entt::null && rename_buffer[0] != '\0') {
                        if (auto world_ptr = context.toolboard.try_get<World*>()) {
                            world_ptr->set_name(SceneNode(entity_to_rename), rename_buffer);
                            if (scene_mgr) scene_mgr->set_dirty(true);
                        }
                    }
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
                        if (entity_to_rename != entt::null && rename_buffer[0] != '\0') {
                            if (auto world_ptr = context.toolboard.try_get<World*>()) {
                                world_ptr->set_name(SceneNode(entity_to_rename), rename_buffer);
                                if (scene_mgr) scene_mgr->set_dirty(true);
                            }
                        }
                        show_rename_modal = false;
                        ui::close_modal();
                    }, ui::ButtonRole::Primary);
                });
            });
        }

        // process deferred duplicate
        if (entity_to_duplicate != entt::null) {
            if (auto world_ptr = context.toolboard.try_get<World*>()) {
                auto new_entity = duplicate_entity(*world_ptr, entity_to_duplicate, entt::null, scene_mgr);
                if (scene_mgr) scene_mgr->set_dirty(true);
                selected_node                                           = SceneTree::INVALID_NODE;
                context.blackboard.get<HierarchyView::Selection>().node = SceneNode(new_entity);
            }
            entity_to_duplicate = entt::null;
        }

        // process deferred delete children
        if (entity_to_delete_children != entt::null) {
            if (auto world_ptr = context.toolboard.try_get<World*>()) {
                auto& world = *world_ptr;
                if (auto* children = world.registry.try_get<Children>(entity_to_delete_children)) {
                    auto nodes = children->nodes;
                    for (auto child : nodes) {
                        world.destroy_tree(child);
                    }
                    world.registry.remove<Children>(entity_to_delete_children);
                }
                if (scene_mgr) scene_mgr->set_dirty(true);
            }
            entity_to_delete_children = entt::null;
        }

        // process deferred deletion
        if (entity_to_delete != entt::null) {
            if (auto world_ptr = context.toolboard.try_get<World*>()) {
                auto& world = *world_ptr;
                world.destroy_tree(SceneNode(entity_to_delete));
                selected_node                                           = SceneTree::INVALID_NODE;
                context.blackboard.get<HierarchyView::Selection>().node = SceneNode{};
                if (scene_mgr) scene_mgr->set_dirty(true);
            }
            entity_to_delete = entt::null;
        }
    });
}
