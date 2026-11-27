#include <cctype>
#include <algorithm>
#include <cxxopts.hpp>

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
#include "Common/EditorMenuBar.h"
#include "Common/EditorCommands.h"

// local renderer
#include "Renderer.h"

using namespace lyra;

static void imgui_update(AppContext& context)
{
    lyra::execute_once([&context]() {
        setup_editor_commands(context);
    });

    render_editor_menubar(context);

    // global shortcuts and command palette modal
    ui::command::dispatch_shortcuts();
    ui::command::palette();
    render_editor_modals(context);
}

static void imgui_render(AppContext& context)
{
    auto device   = context.services.get<GPUDevice*>();
    auto surface  = context.services.get<GPUSurface*>();
    auto renderer = context.services.get<EditorRenderer*>();

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

    // unified frame graph execution (scene + imgui)
    renderer->render(backbuffer, context, command);

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
        ("p,project", "project root directory", cxxopts::value<std::filesystem::path>()->default_value("Scratch/project"))
        ("gfxapi", "graphics API backend (vulkan, d3d12, metal)", cxxopts::value<std::string>()->default_value("vulkan"))
        ("renderer", "renderer backend (forward, deferred, gpudriven, pathtracer)", cxxopts::value<std::string>()->default_value("forward"))
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

    auto engine_root        = git_root().value_or(Path(".")) / "Engine";
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
        desc.with_mount("sys://", engine_caches_root, 1);
        desc.with_mount("sys://", engine_assets_root, 0);
        desc.with_mount("app://", caches_root, 1);
        desc.with_mount("app://", assets_root, 0);
        return std::make_unique<Application>(desc);
    });

    // asset layer
    auto assets = lyra::execute([&]() {
        auto desc            = AMSDescriptor{};
        desc.sys.assets_path = engine_assets_root.c_str();
        desc.sys.caches_path = engine_caches_root.c_str();
        desc.app.assets_path = assets_root.c_str();
        desc.app.caches_path = caches_root.c_str();
        desc.loader          = app->get_file_loader();
        desc.registry        = registry.c_str();
        desc.watch           = true;

        auto layer = std::make_unique<AssetLayer>(desc);
        app->bind(*layer);

        auto ams = app->get_services().get<AssetServer*>();

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
        desc.window    = *app->get_services().get<Window*>();
        desc.surface   = *app->get_services().get<GPUSurface*>();
        desc.compiler  = *app->get_services().get<Compiler*>();
        desc.docking   = true;
        desc.viewports = false;

        auto layer = std::make_unique<UILayer>(desc);
        layer->apply_context(); // inlined into executable, sets GImGui in lyra-editor.exe
        app->bind(*layer);
        return std::move(layer);
    });

    // layout layer
    auto layout = lyra::execute([&]() {
        auto desc          = EditorLayoutDescriptor{};
        desc.layout.left   = 0.20f;
        desc.layout.right  = 0.25f;
        desc.layout.bottom = 0.35f;

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

    // editor viewport renderer
    auto renderer = std::make_unique<EditorRenderer>();
    app->bind<EditorRenderer>(*renderer);

    // bind additional systems
    app->bind<AppEvent::UPDATE, &imgui_update>();
    app->bind<AppEvent::RENDER, &imgui_render>();

    // event loop
    app->run();
    return EXIT_SUCCESS;
}
