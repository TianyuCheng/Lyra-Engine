#include <cctype>
#include <algorithm>
#include <cxxopts.hpp>
#include <imgui.h>

// uber header for lyra engine
#include <Lyra/Lyra.hpp>

// local editor ui headers
#include "Panels/AssetBrowserView.h"
#include "Panels/ConsoleView.h"
#include "Panels/HierarchyView.h"
#include "Panels/InspectorView.h"
#include "Panels/SceneView.h"

// other common headers
#include "Common/EditorLayout.h"

// local renderer
#include "Renderer.h"

using namespace lyra;

static void render_scene(AppContext& context, GPUCommandBuffer command)
{
    // apply a toy demo renderer
    if (auto* view = context.toolboard.try_get<SceneView>()) {
        auto* renderer = context.toolboard.get<SampleCubeRenderer*>();
        renderer->render(view->get_backbuffer(), context, command);
    }
}

static bool show_new_scene_modal = false;
static bool open_new_scene_modal = false;
static char new_scene_name[128]  = "Untitled";

static void imgui_update(AppContext& context)
{
    auto* scene_mgr = context.toolboard.try_get<SceneManager>();

    auto trigger_new_scene = [&]() {
        if (scene_mgr && scene_mgr->is_dirty()) {
            if (!ui::dialog::confirm("Unsaved Changes", "The active scene has unsaved changes. Discard and create a new scene?")) {
                return;
            }
        }
        strcpy_s(new_scene_name, sizeof(new_scene_name), "Untitled");
        show_new_scene_modal = true;
        open_new_scene_modal = true;
    };

    auto reset_input_states = [&]() {
        if (auto input = context.toolboard.try_get<InputManager*>()) {
            input->reset();
        }
        if (auto window = context.toolboard.try_get<Window*>()) {
            window->reset_input_state();
        }
        ui::clear_input_keys();
    };

    auto to_app_vpath = [&](const Path& os_path) -> Optional<String> {
        if (auto* browser = context.toolboard.try_get<AssetBrowserView>()) {
            std::error_code ec;
            Path rel = fs::relative(os_path, browser->get_app_root(), ec);
            if (!ec && !rel.empty() && *rel.begin() != "..") {
                return VFSPath(vfs::SchemeApp, rel.generic_string()).to_string();
            }
        }
        return std::nullopt;
    };

    auto do_save_scene_as = [&]() {
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
    };

    auto do_save_scene = [&]() {
        if (!scene_mgr) return;
        if (!scene_mgr->get_active_path().empty()) {
            if (!scene_mgr->save_active()) {
                ui::dialog::alert("Save Failed", "Failed to save active scene:\n" + scene_mgr->get_active_path().string(), ui::StatusRole::Error);
            }
            reset_input_states();
        } else {
            do_save_scene_as();
        }
    };

    auto do_open_scene = [&]() {
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
    };

    ui::menubar([&]() {
        ui::menu("Scene", [&]() {
            ui::menu_item("New Scene", "Ctrl+N", [&]() {
                trigger_new_scene();
            });

            ui::menu_item("Open Scene...", "Ctrl+O", [&]() {
                do_open_scene();
            });

            ui::menu_item("Save Scene", "Ctrl+S", [&]() {
                do_save_scene();
            });

            ui::menu_item("Save Scene As...", "Ctrl+Shift+S", [&]() {
                do_save_scene_as();
            });

            ui::separator();

            ui::menu_item("Load Additive...", [&]() {
                if (scene_mgr) {
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
                }
            });
        });

        if (auto* ams = context.toolboard.try_get<AssetServer>()) {
            ui::menu("Assets", [&]() {
                ui::menu_item("Reimport All (Force)", [&]() {
                    ams->reimport_all(true);
                });
                ui::menu_item("Purge Unused Assets", [&]() {
                    ams->purge();
                });
                ui::menu_item("Flush Registry to Disk", [&]() {
                    ams->flush();
                });
                ui::separator();
                ui::menu_check_item("Watch Source Directory", ams->is_watching(), [&](const bool& watching) {
                    ams->set_watching(watching);
                });
            });
        }
    });

    // keyboard shortcuts
    if (!ui::is_text_input_active() && scene_mgr) {
        bool ctrl  = ui::is_key_down(KeyButton::CTRL);
        bool shift = ui::is_key_down(KeyButton::SHIFT);

        if (ctrl && !shift && ui::is_key_pressed(KeyButton::N)) {
            reset_input_states();
            trigger_new_scene();
        } else if (ctrl && !shift && ui::is_key_pressed(KeyButton::O)) {
            reset_input_states();
            do_open_scene();
        } else if (ctrl && !shift && ui::is_key_pressed(KeyButton::S)) {
            reset_input_states();
            do_save_scene();
        } else if (ctrl && shift && ui::is_key_pressed(KeyButton::S)) {
            reset_input_states();
            do_save_scene_as();
        }
    }

    if (open_new_scene_modal) {
        ui::open_modal(LYRA_ICON_SCENE " New Scene");
        open_new_scene_modal = false;
    }

    if (show_new_scene_modal) {
        ui::modal(LYRA_ICON_SCENE " New Scene", &show_new_scene_modal, [&]() {
            ui::label("Enter scene name:");
            ui::text_field("##new_scene_name", new_scene_name, sizeof(new_scene_name), [&]() {
                if (new_scene_name[0] != '\0' && scene_mgr) {
                    scene_mgr->create(new_scene_name);
                }
                show_new_scene_modal = false;
                ui::close_modal();
            });

            ui::separator();

            ui::row(ui::Alignment::End, [&]() {
                ui::button("Cancel", [&]() {
                    show_new_scene_modal = false;
                    ui::close_modal();
                });

                ui::button("Create", [&]() {
                    if (new_scene_name[0] != '\0' && scene_mgr) {
                        scene_mgr->create(new_scene_name);
                    }
                    show_new_scene_modal = false;
                    ui::close_modal();
                },
                    ui::ButtonRole::Primary);
            });
        });
    }
}

static void imgui_render(AppContext& context)
{
    auto device   = context.toolboard.get<GPUDevice*>();
    auto surface  = context.toolboard.get<GPUSurface*>();
    auto renderer = context.toolboard.get<GUIRenderer*>();

    // command buffer
    auto command = lyra::execute([&]() {
        auto desc  = GPUCommandBufferDescriptor{};
        desc.queue = GPUQueueType::DEFAULT;
        return device->create_command_buffer(desc);
    });

    // current backbuffer
    auto backbuffer = surface->get_current_texture();

    // synchronization
    command.wait(backbuffer.available, GPUBarrierSync::PIXEL_SHADING);
    command.signal(backbuffer.complete, GPUBarrierSync::RENDER_TARGET);

    // render scene command encoding
    render_scene(context, command);

    // render UI command recording
    command.resource_barrier(state_transition(backbuffer.texture, undefined_texture_state(), color_attachment_texture_state()));
    renderer->render_main_viewport(command, backbuffer.view);
    command.resource_barrier(state_transition(backbuffer.texture, color_attachment_texture_state(), present_src_texture_state()));

    // command buffer submission
    command.submit();

    // swapchain presentation
    backbuffer.present();
}

// attach editor camera controller (FlyCamera) to scene cameras
// NOTE: This is NOT a long-term solution.
static void attach_editor_camera(World& world)
{
    auto view = world.registry.view<Camera>();
    for (auto entity : view) {
        if (!world.registry.any_of<FlyCamera>(entity)) {
            world.add_component<FlyCamera>(SceneNode(entity),
                FlyCamera{
                    .pitch        = -20.0f,
                    .target_pitch = -20.0f,
                });
        }
    }
};

int main(int argc, const char* argv[])
{
    // clang-format off
    cxxopts::Options options("Lyra::Editor", "Lyra engine editor program.");
    options.add_options()
        ("p,project", "project root directory", cxxopts::value<std::filesystem::path>()->default_value("Scratch/project"))
        ("gfxapi", "graphics API backend (vulkan, d3d12, metal)", cxxopts::value<std::string>()->default_value("vulkan"))
        ("h,help", "print usage")
    ;
    // clang-format on

    // parse arguments
    auto args = options.parse(argc, argv);
    if (args.count("help")) {
        spdlog::error("{}", options.help());
        exit(0);
    }

    auto gfxapi = args["gfxapi"].as<std::string>();
    std::transform(gfxapi.begin(), gfxapi.end(), gfxapi.begin(), ::tolower);

    auto backend = RHIBackend::VULKAN;
    if (gfxapi == "d3d12") {
        backend = RHIBackend::D3D12;
    } else if (gfxapi == "metal") {
        backend = RHIBackend::METAL;
    } else if (gfxapi == "vulkan") {
        backend = RHIBackend::VULKAN;
    } else {
        spdlog::warn("Unknown graphics API '{}', defaulting to vulkan", gfxapi);
    }

    // common paths
    auto root = args["project"].as<Path>();
    if (!fs::exists(root))
        std::filesystem::create_directory(root);

    auto assets_root = root / "Assets";
    if (!fs::exists(assets_root))
        fs::create_directory(assets_root);

    auto caches_root = root / "Caches";
    if (!fs::exists(caches_root))
        fs::create_directory(caches_root);

    auto registry = root / "Assets.toml";

    auto engine_root        = git_root() / "Engine";
    auto engine_assets_root = engine_root / "Assets";
    if (!fs::exists(engine_assets_root))
        fs::create_directories(engine_assets_root);

    auto engine_caches_root = engine_root / "Caches";
    if (!fs::exists(engine_caches_root))
        fs::create_directories(engine_caches_root);

    // application
    auto app = lyra::execute([&]() {
        auto desc = AppDescriptor();
        desc.with_title("Lyra Engine :: Editor");
        desc.with_window_extent(1920, 1080);
        desc.with_window_maximized();
        desc.with_graphics_backend(backend);
        desc.with_graphics_validation(true, true);
        return std::make_unique<Application>(desc);
    });

    // file loader
    auto file_loader = lyra::execute([&]() {
        auto loader = std::make_unique<FileLoader>(FSLoader::NATIVE);
        loader->mount("sys://", engine_caches_root, 1);
        loader->mount("sys://", engine_assets_root, 0);
        loader->mount("app://", caches_root, 1);
        loader->mount("app://", assets_root, 0);
        app->get_toolboard().add<FileLoader*>(loader.get());
        return loader;
    });

    // asset layer
    auto assets = lyra::execute([&]() {
        auto desc            = AMSDescriptor{};
        desc.sys.assets_path = engine_assets_root.c_str();
        desc.sys.caches_path = engine_caches_root.c_str();
        desc.app.assets_path = assets_root.c_str();
        desc.app.caches_path = caches_root.c_str();
        desc.loader          = file_loader.get();
        desc.registry        = registry.c_str();
        desc.watch           = true;

        auto layer = std::make_unique<AssetLayer>(desc);
        app->bind(*layer);

        auto ams = app->get_toolboard().get<AssetServer*>();

        // register assets loaders
        ams->register_asset<TextAsset>();
        ams->register_asset<JsonAsset>();
        ams->register_asset<TomlAsset>();
        ams->register_asset<MeshAsset>();
        ams->register_asset<MaterialAsset>();
        ams->register_asset<SceneAsset>();

        // register multiple cookers for texture asset
        ams->register_asset<TextureAsset>();
        ams->register_asset<TextureAsset, TextureAsset::stb>();
        ams->register_asset<TextureAsset, TextureAsset::exr>();
        ams->register_asset<TextureAsset, TextureAsset::dds>();
        ams->register_asset<TextureAsset, TextureAsset::ktx>();

        // register multiple cookers for models
        ams->register_asset<ModelAsset>();
        ams->register_asset<ModelAsset, ModelAsset::stl>();
        ams->register_asset<ModelAsset, ModelAsset::obj>();
        ams->register_asset<ModelAsset, ModelAsset::gltf>();

        return std::move(layer);
    });

    // timing layer
    auto timing = lyra::execute([&]() {
        auto layer = std::make_unique<TimingLayer>();
        app->bind(*layer);
        return std::move(layer);
    });

    // input layer
    auto input = lyra::execute([&]() {
        auto layer = std::make_unique<InputLayer>();
        app->bind(*layer);
        return std::move(layer);
    });

    // render layer (owns GPU deletion queue + camera ECS systems)
    auto render = lyra::execute([&]() {
        auto layer = std::make_unique<RenderLayer>();
        app->bind(*layer);
        return std::move(layer);
    });

    // scene layer
    auto scene = lyra::execute([&]() {
        auto layer = std::make_unique<SceneLayer>();
        app->bind(*layer);

        attach_editor_camera(layer->get_world());
        return std::move(layer);
    });

    // script layer
    auto scripting = lyra::execute([&]() {
        auto layer = std::make_unique<ScriptLayer>();
        layer->set_simulation_state(SimulationState::EDIT);
        app->bind(*layer);
        return std::move(layer);
    });

    // ui layer
    auto uilayer = lyra::execute([&]() {
        auto desc      = GUIDescriptor{};
        desc.window    = *app->get_toolboard().get<Window*>();
        desc.surface   = *app->get_toolboard().get<GPUSurface*>();
        desc.compiler  = *app->get_toolboard().get<Compiler*>();
        desc.docking   = true;
        desc.viewports = false;

        auto layer = std::make_unique<UILayer>(desc);
        layer->apply_context(); // inlined into executable, sets GImGui in lyra-editor.exe
        app->bind(*layer);
        return std::move(layer);
    });

    // layout layer
    auto layout = lyra::execute([&]() {
        auto desc   = EditorLayoutDescriptor{};
        desc.left   = 0.2f;
        desc.right  = 0.3f;
        desc.top    = 0.2f;
        desc.bottom = 0.4f;

        auto layer = std::make_unique<EditorLayout>(desc);
        app->bind(*layer);
        return std::move(layer);
    });

    // editor components (console logs)
    auto console = std::make_unique<ConsoleView>(4096);
    app->bind<ConsoleView>(*console);

    // editor components (asset browser)
    auto files = std::make_unique<AssetBrowserView>(assets_root, engine_assets_root);
    app->bind<AssetBrowserView>(*files);

    // editor components (object inspector)
    auto inspector = std::make_unique<InspectorView>();
    app->bind<InspectorView>(*inspector);

    // editor components (scene tree hierarchy)
    auto hierarchy = std::make_unique<HierarchyView>();
    app->bind<HierarchyView>(*hierarchy);

    // editor components (scene)
    auto sceneview = std::make_unique<SceneView>();
    app->bind<SceneView>(*sceneview);

    // renderer (temporary solution)
    auto renderer = std::make_unique<SampleCubeRenderer>();
    app->bind<SampleCubeRenderer>(*renderer);

    // bind additional systems
    app->bind<AppEvent::UPDATE, &imgui_update>();
    app->bind<AppEvent::RENDER, &imgui_render>();

    // event loop
    app->run();
    return EXIT_SUCCESS;
}
