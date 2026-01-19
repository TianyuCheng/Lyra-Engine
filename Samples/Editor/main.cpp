#include <cxxopts.hpp>
#include <Lyra/Lyra.hpp>
#include "renderer.h"

using namespace lyra;

static void render_scene(Blackboard& blackboard, GPUCommandBuffer command)
{
    // apply a toy demo renderer
    if (auto view = blackboard.try_get<SceneView*>()) {
        auto device   = blackboard.get<GPUDevice>();
        auto renderer = blackboard.get<SampleCubeRenderer*>();
        renderer->render((*view)->get_backbuffer(), device, command);
    }
}

static void imgui_update(Blackboard& blackboard)
{
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Project")) {
            if (ImGui::MenuItem("New")) {
            }
            if (ImGui::MenuItem("Load", "Ctrl+O")) {
            }
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    lyra::execute_once([&]() {
        auto& layout = blackboard.get<EditorLayoutInfo>();
        ImGui::DockBuilderDockWindow("Dear ImGui Demo", layout.main);
    });

    ImGui::ShowDemoWindow();
}

static void imgui_render(Blackboard& blackboard)
{
    auto device  = blackboard.get<GPUDevice>();
    auto surface = blackboard.get<GPUSurface>();
    auto imgui   = blackboard.get<GUIRenderer*>();

    // command buffer
    auto command = lyra::execute([&]() {
        auto desc  = GPUCommandBufferDescriptor{};
        desc.queue = GPUQueueType::DEFAULT;
        return device.create_command_buffer(desc);
    });

    // current backbuffer
    auto backbuffer = surface.get_current_texture();

    // synchronization
    command.wait(backbuffer.available, GPUBarrierSync::PIXEL_SHADING);
    command.signal(backbuffer.complete, GPUBarrierSync::RENDER_TARGET);

    // render scene command encoding
    render_scene(blackboard, command);

    // render UI command recording
    command.resource_barrier(state_transition(backbuffer.texture, undefined_state(), color_attachment_state()));
    imgui->render_main_viewport(command, backbuffer.view);
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
        ("p,project", "project root directory", cxxopts::value<std::filesystem::path>()->default_value("./"))
        ("h,help", "print usage")
    ;
    // clang-format on

    // parse arguments
    auto args = options.parse(argc, argv);
    if (args.count("help")) {
        spdlog::error("{}", options.help());
        exit(0);
    }

    // common paths
    auto root = args["project"].as<Path>();
    if (!fs::exists(root))
        std::filesystem::create_directory(root);

    auto assets_root = root / "Assets";
    if (!fs::exists(assets_root))
        fs::create_directory(assets_root);

    auto metadata_root = root / "Metadata";
    if (!fs::exists(metadata_root))
        fs::create_directory(metadata_root);

    auto generated_root = root / "Generated";
    if (!fs::exists(generated_root))
        fs::create_directory(generated_root);

    // application
    auto app = lyra::execute([&]() {
        auto desc = AppDescriptor();
        desc.with_title("Lyra Engine :: Editor");
        desc.with_window_extent(1920, 1080);
        desc.with_window_maximized();
        desc.with_graphics_backend(RHIBackend::VULKAN);
        desc.with_graphics_validation(true, true);
        return std::make_unique<Application>(desc);
    });

    // file loader
    auto file_loader = lyra::execute([&]() {
        auto loader = std::make_unique<FileLoader>(FSLoader::NATIVE);
        loader->mount("/", generated_root, 2);
        loader->mount("/", metadata_root, 1);
        loader->mount("/", assets_root, 0);
        return loader;
    });

    // asset layer
    auto assets = lyra::execute([&]() {
        auto desc                    = AMSDescriptor{};
        desc.importer.assets_path    = assets_root.c_str();
        desc.importer.metadata_path  = metadata_root.c_str();
        desc.importer.generated_path = generated_root.c_str();
        desc.loader.assets           = file_loader.get();
        desc.loader.metadata         = file_loader.get();
        desc.watch                   = true;
        desc.workers                 = 4;

        auto layer = std::make_unique<AssetLayer>(desc);
        app->bind(*layer);
        return std::move(layer);
    });

    // imgui layer
    auto imgui = lyra::execute([&]() {
        auto desc      = GUIDescriptor{};
        desc.window    = app->get_blackboard().get<Window>();
        desc.surface   = app->get_blackboard().get<GPUSurface>();
        desc.compiler  = app->get_blackboard().get<Compiler>();
        desc.docking   = true;
        desc.viewports = false;

        auto layer = std::make_unique<ImGuiLayer>(desc);
        layer->apply_context(); // imgui context in user application
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

    // editor components (console)
    auto console = std::make_unique<Console>(4096);
    app->bind<Console>(*console);

    // editor components (files)
    auto files = std::make_unique<Files>(assets_root);
    app->bind<Files>(*files);

    // editor components (inspector)
    auto inspector = std::make_unique<Inspector>();
    app->bind<Inspector>(*inspector);

    // editor components (hierarchy)
    auto hierarchy = std::make_unique<Hierarchy>();
    app->bind<Hierarchy>(*hierarchy);

    // editor components (scene)
    auto scene = std::make_unique<SceneView>();
    app->bind<SceneView>(*scene);

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
