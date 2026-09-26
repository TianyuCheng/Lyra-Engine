#include <cctype>
#include <algorithm>
#include <cxxopts.hpp>
#include <Lyra/Lyra.hpp>
#include <Lyra/Scene/Camera.h>

#include "Renderer.h"
#include "Common/EditorLayout.h"
#include "Panels/AssetBrowserView.h"
#include "Panels/ConsoleView.h"
#include "Panels/HierarchyView.h"
#include "Panels/InspectorView.h"
#include "Panels/SceneView.h"

using namespace lyra;

static void render_scene(AppContext& context, GPUCommandBuffer command)
{
    // apply a toy demo renderer
    if (auto* view = context.toolboard.try_get<SceneView>()) {
        auto* renderer = context.toolboard.get<SampleCubeRenderer*>();
        renderer->render(view->get_backbuffer(), context, command);
    }
}

static void imgui_update(AppContext& context)
{
    ui::menubar([&]() {
        ui::menu("Project", [&]() {
            ui::menu_item("New", [&]() {});
            ui::menu_item("Load", "Ctrl+O", [&]() {});
            ui::menu_item("Save", "Ctrl+S", [&]() {});
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
    command.resource_barrier(state_transition(backbuffer.texture, undefined_state(), color_attachment_state()));
    renderer->render_main_viewport(command, backbuffer.view);
    command.resource_barrier(state_transition(backbuffer.texture, color_attachment_state(), present_src_state()));

    // command buffer submission
    command.submit();

    // swapchain presentation
    backbuffer.present();
}

int main(int argc, const char* argv[])
{
    // clang-format off
    cxxopts::Options options("Lyra::Editor", "Lyra engine editor program.");
    options.add_options()
        ("p,project", "project root directory", cxxopts::value<std::filesystem::path>())
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
        loader->mount("/", caches_root, 1);
        loader->mount("/", assets_root, 0);
        app->get_toolboard().add<FileLoader*>(loader.get());
        return loader;
    });

    // asset layer
    auto assets = lyra::execute([&]() {
        auto desc                 = AMSDescriptor{};
        desc.importer.assets_path = assets_root.c_str();
        desc.importer.caches_path = caches_root.c_str();
        desc.loader.assets        = file_loader.get();
        desc.loader.caches        = file_loader.get();
        desc.registry             = registry.c_str();
        desc.watch                = true;

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
        return std::move(layer);
    });

    // script layer
    auto scripting = lyra::execute([&]() {
        auto layer = std::make_unique<ScriptLayer>();
        layer->set_simulation_state(SimulationState::EDIT);
        layer->register_api(scripts::camera::create());
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
        layer->apply_context(); // ui context in user application
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
    auto files = std::make_unique<AssetBrowserView>(assets_root);
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
