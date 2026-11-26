#include "helper.h"

#include <imgui.h>
#include <Lyra/UISystem/Renderer/GUITypes.h>
#include <Lyra/UISystem/Widgets/UI.h>
#include <Lyra/UISystem/Widgets/UIControls.h>
#include <Lyra/UISystem/Widgets/UIProperty.h>
#include <Lyra/UISystem/Widgets/UINodeEditor.h>
#include <Lyra/UISystem/Widgets/UINodeEnums.h>

using namespace lyra;

static void apply_theme()
{
    ImGuiStyle& style  = ImGui::GetStyle();
    ImVec4*     colors = style.Colors;

    // ui config
    style.WindowPadding        = ImVec2(10.0f, 10.0f);
    style.FramePadding         = ImVec2(8.0f, 6.0f);
    style.ItemSpacing          = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing     = ImVec2(6.0f, 6.0f);
    style.GrabMinSize          = 10.0f;
    style.DockingSeparatorSize = 6.0f;
    style.IndentSpacing        = 20.0f;

    // corners
    style.WindowRounding    = 6.0f;
    style.ChildRounding     = 6.0f;
    style.FrameRounding     = 4.0f;
    style.PopupRounding     = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding      = 4.0f;
    style.TabRounding       = 4.0f;

    // colors
    colors[ImGuiCol_Text]                 = ImVec4(0.92f, 0.93f, 0.94f, 1.00f);
    colors[ImGuiCol_TextDisabled]         = ImVec4(0.50f, 0.52f, 0.54f, 1.00f);
    colors[ImGuiCol_WindowBg]             = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
    colors[ImGuiCol_ChildBg]              = ImVec4(0.16f, 0.16f, 0.18f, 1.00f);
    colors[ImGuiCol_PopupBg]              = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
    colors[ImGuiCol_Border]               = ImVec4(0.28f, 0.29f, 0.30f, 0.60f);
    colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]              = ImVec4(0.20f, 0.22f, 0.24f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.22f, 0.24f, 0.26f, 1.00f);
    colors[ImGuiCol_FrameBgActive]        = ImVec4(0.24f, 0.26f, 0.28f, 1.00f);
    colors[ImGuiCol_TitleBg]              = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
    colors[ImGuiCol_TitleBgActive]        = ImVec4(0.16f, 0.16f, 0.18f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
    colors[ImGuiCol_MenuBarBg]            = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.16f, 0.16f, 0.18f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.24f, 0.26f, 0.28f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.28f, 0.30f, 0.32f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.32f, 0.34f, 0.36f, 1.00f);
    colors[ImGuiCol_CheckMark]            = ImVec4(0.46f, 0.56f, 0.66f, 1.00f);
    colors[ImGuiCol_SliderGrab]           = ImVec4(0.36f, 0.46f, 0.56f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.40f, 0.50f, 0.60f, 1.00f);
    colors[ImGuiCol_Button]               = ImVec4(0.24f, 0.34f, 0.44f, 1.00f);
    colors[ImGuiCol_ButtonHovered]        = ImVec4(0.28f, 0.38f, 0.48f, 1.00f);
    colors[ImGuiCol_ButtonActive]         = ImVec4(0.32f, 0.42f, 0.52f, 1.00f);
    colors[ImGuiCol_Header]               = ImVec4(0.24f, 0.34f, 0.44f, 1.00f);
    colors[ImGuiCol_HeaderHovered]        = ImVec4(0.28f, 0.38f, 0.48f, 1.00f);
    colors[ImGuiCol_HeaderActive]         = ImVec4(0.32f, 0.42f, 0.52f, 1.00f);
    colors[ImGuiCol_Separator]            = ImVec4(0.28f, 0.29f, 0.30f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]     = ImVec4(0.46f, 0.56f, 0.66f, 1.00f);
    colors[ImGuiCol_SeparatorActive]      = ImVec4(0.46f, 0.56f, 0.66f, 1.00f);
}

struct UINodeEditorApp : public TestApp
{
    OwnedResource<GUIRenderer> gui;

    explicit UINodeEditorApp(const TestAppDescriptor& desc) : TestApp(desc)
    {
        GUIDescriptor gui_desc{};
        gui_desc.compiler  = *compiler;
        gui_desc.width     = desc.width;
        gui_desc.height    = desc.height;
        gui_desc.font_size = 14.0f;
        gui_desc.docking   = false;
        gui_desc.viewports = false;
        gui                = GUIRenderer::init(gui_desc);
    }

    ~UINodeEditorApp()
    {
        RHI::get_current_device().wait();
        gui.reset();
    }

    void render_ui()
    {
        ui::set_context(gui->context<void>());
        ImGui::SetCurrentContext(gui->context<ImGuiContext>());

        ImGuiIO& io    = ImGui::GetIO();
        io.IniFilename = nullptr; // disable ini persistence for reproducible test output
        io.DeltaTime   = 1.0f / 60.0f;
        io.DisplaySize = ImVec2(static_cast<float>(desc.width), static_cast<float>(desc.height));

        gui->new_frame();
        apply_theme();

        ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(static_cast<float>(desc.width) - 20.0f, static_cast<float>(desc.height) - 20.0f), ImGuiCond_Always);

        ui::panel("Material Graph Editor", [&]() {
            ui::node::editor("material_graph", [&]() {
                ui::node::reset_panning(Vector2(0.0f, 0.0f));

                // ensure deterministic positions on the canvas
                ui::node::set_node_position(1, Vector2(40.0f, 50.0f), ui::node::CoordinateSpace::Grid);
                ui::node::set_node_position(2, Vector2(40.0f, 250.0f), ui::node::CoordinateSpace::Grid);
                ui::node::set_node_position(3, Vector2(420.0f, 90.0f), ui::node::CoordinateSpace::Grid);
                // Node 1: Texture Sample
                ui::node::node(1, [&]() {
                    ui::node::node_title([&]() {
                        ui::label("Texture Sample");
                    });

                    ui::node::input_pin(11, ui::node::PinShape::Circle, [&]() {
                        ui::label("UV");
                    });

                    ui::node::output_pin(12, ui::node::PinShape::CircleFilled, [&]() {
                        ui::label("RGBA");
                    });

                    ui::node::output_pin(13, ui::node::PinShape::CircleFilled, [&]() {
                        ui::label("Alpha");
                    });
                });

                // Node 2: Fresnel
                ui::node::node(2, [&]() {
                    ui::node::node_title([&]() {
                        ui::label("Fresnel Effect");
                    });

                    ui::node::input_pin(21, ui::node::PinShape::Circle, [&]() {
                        ui::label("Normal");
                    });

                    ui::node::input_pin(22, ui::node::PinShape::Circle, [&]() {
                        ui::label("Power");
                    });

                    ui::node::output_pin(23, ui::node::PinShape::CircleFilled, [&]() {
                        ui::label("Out");
                    });
                });

                // Node 3: PBR Surface Output
                ui::node::node(3, [&]() {
                    ui::node::node_title([&]() {
                        ui::label("PBR Material");
                    });

                    ui::node::input_pin(31, ui::node::PinShape::CircleFilled, [&]() {
                        ui::label("Base Color");
                    });

                    ui::node::input_pin(32, ui::node::PinShape::CircleFilled, [&]() {
                        ui::label("Roughness");
                    });

                    ui::node::input_pin(33, ui::node::PinShape::Circle, [&]() {
                        ui::label("Metallic");
                    });

                    ui::node::static_attribute(34, [&]() {
                        static float emission = 0.5f;
                        ui::slider("Emission", &emission, 0.0f, 1.0f);
                    });

                    ui::node::output_pin(35, ui::node::PinShape::QuadFilled, [&]() {
                        ui::label("Surface Output");
                    });
                });

                // Connections
                ui::node::link(1, 12, 31); // RGBA -> Base Color
                ui::node::link(2, 23, 32); // Fresnel Out -> Roughness

                // Minimap
                ui::node::minimap(0.2f, ui::node::MinimapLocation::BottomRight);
            });
        });

        gui->end_frame();
    }

    void update(const WindowInput&) override
    {
        // warm-up frame for layout and positioning
        render_ui();
        // snapshot frame
        render_ui();
    }

    void render(const GPUSurfaceTexture& backbuffer) override
    {
        auto& device = RHI::get_current_device();

        GPUCommandBufferDescriptor cmd_desc{};
        cmd_desc.queue = GPUQueueType::DEFAULT;
        auto cmd       = device.create_command_buffer(cmd_desc);

        cmd.resource_barrier(state_transition(backbuffer.texture, undefined_texture_state(), color_attachment_texture_state()));
        gui->render_main_viewport(cmd, backbuffer.view);
        postprocessing(cmd, backbuffer.texture);
        cmd.submit();
    }
};

TEST_CASE("rhi::vulkan::ui_node_editor" * doctest::description("Rendering node editor graph, nodes, pins, links, and minimap."))
{
    TestAppDescriptor desc{};
    desc.name          = "vulkan";
    desc.window        = false;
    desc.backend       = RHIBackend::VULKAN;
    desc.width         = 960;
    desc.height        = 540;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    UINodeEditorApp(desc).run();
}

#ifdef WIN32
TEST_CASE("rhi::d3d12::ui_node_editor" * doctest::description("Rendering node editor graph, nodes, pins, links, and minimap."))
{
    TestAppDescriptor desc{};
    desc.name          = "d3d12";
    desc.window        = false;
    desc.backend       = RHIBackend::D3D12;
    desc.width         = 960;
    desc.height        = 540;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    UINodeEditorApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::ui_node_editor" * doctest::description("Rendering node editor graph, nodes, pins, links, and minimap."))
{
    TestAppDescriptor desc{};
    desc.name          = "metal";
    desc.window        = false;
    desc.backend       = RHIBackend::METAL;
    desc.width         = 960;
    desc.height        = 540;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    UINodeEditorApp(desc).run();
}
#endif

TEST_CASE("rhi::softgpu::ui_node_editor" * doctest::description("Rendering node editor graph, nodes, pins, links, and minimap."))
{
    TestAppDescriptor desc{};
    desc.name          = "softgpu";
    desc.window        = false;
    desc.backend       = RHIBackend::SOFTGPU;
    desc.width         = 960;
    desc.height        = 540;
    desc.rhi_flags     = RHIFlag::DEBUG;
    desc.compile_flags = CompileFlag::DEBUG;
    UINodeEditorApp(desc).run();
}
