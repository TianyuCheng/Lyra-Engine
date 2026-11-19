#include "helper.h"

#include <imgui.h>
#include <Lyra/UISystem/Renderer/GUITypes.h>
#include <Lyra/UISystem/Widgets/UI.h>
#include <Lyra/UISystem/Widgets/UIControls.h>
#include <Lyra/UISystem/Widgets/UIProperty.h>
#include <Lyra/UISystem/Widgets/UITree.h>

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

struct UISystemApp : public TestApp
{
    OwnedResource<GUIRenderer> gui;

    explicit UISystemApp(const TestAppDescriptor& desc) : TestApp(desc)
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

    ~UISystemApp()
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

        // panel 1: UI Controls
        ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(460.0f, 520.0f), ImGuiCond_Always);
        ui::panel("UI Controls", [&]() {
            ui::button("Standard Button", []() {
            },
                ui::ButtonRole::Standard);
            ui::button("Primary Action", []() {
            },
                ui::ButtonRole::Primary);
            ui::button("Delete Item", []() {
            },
                ui::ButtonRole::Danger);

            ui::checkbox("Enable Lighting", true);
            ui::checkbox("Wireframe Mode", false);

            static bool toggle_val = true;
            ui::toggle_switch("hdr_toggle", &toggle_val, "Toggle HDR");

            static float slider_val = 45.0f;
            ui::slider("FOV", &slider_val, 10.0f, 120.0f);

            ui::label("Engine State: Running", ui::StatusRole::Success);
            ui::label("Performance: 60 FPS", ui::StatusRole::Info);
            ui::label("Warning: Test Mode", ui::StatusRole::Warning);
        });

        // panel 2: Properties & Scene Hierarchy
        ImGui::SetNextWindowPos(ImVec2(480.0f, 10.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(470.0f, 520.0f), ImGuiCond_Always);
        ui::panel("Properties & Hierarchy", [&]() {
            ui::section("Transform", "", [&]() {
                ui::properties([&]() {
                    Vector3 pos(1.0f, 2.5f, -3.0f);
                    Vector3 rot(0.0f, 45.0f, 90.0f);
                    Vector3 scale(1.0f, 1.0f, 1.0f);
                    float   roughness = 0.35f;
                    ui::vec3("Position", pos);
                    ui::vec3("Rotation", rot);
                    ui::vec3("Scale", scale);
                    ui::number("Roughness", roughness);
                });
            });

            ui::section("Scene Graph", "", [&]() {
                ui::TreeItemDescriptor root_desc{};
                root_desc.id    = 1;
                root_desc.label = "Scene Root";
                root_desc.flags = ui::TreeNodeFlag::DefaultOpen | ui::TreeNodeFlag::SpanFullWidth;
                ui::tree_node(root_desc, [&]() {
                    ui::tree_node(2, "Main Camera", nullptr, true, true);
                    ui::tree_node(3, "Directional Light", nullptr, false, true);

                    ui::TreeItemDescriptor group_desc{};
                    group_desc.id    = 4;
                    group_desc.label = "Geometry Group";
                    group_desc.flags = ui::TreeNodeFlag::DefaultOpen | ui::TreeNodeFlag::SpanFullWidth;
                    ui::tree_node(group_desc, [&]() {
                        ui::tree_node(5, "Submesh_0", nullptr, false, true);
                        ui::tree_node(6, "Submesh_1", nullptr, false, true);
                    });
                });
            });
        });

        gui->end_frame();
    }

    void update(const WindowInput&) override
    {
        // warm-up frame so auto-fit measurements (such as ui::properties column width) settle into steady state
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

TEST_CASE("rhi::vulkan::ui_system" * doctest::description("Rendering ImGui UI controls, properties, and tree nodes."))
{
    TestAppDescriptor desc{};
    desc.name           = "vulkan";
    desc.window         = false;
    desc.backend        = RHIBackend::VULKAN;
    desc.width          = 960;
    desc.height         = 540;
    desc.rhi_flags      = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_target = CompileTarget::SPIRV;
    desc.compile_flags  = CompileFlag::DEBUG;
    UISystemApp(desc).run();
}

#ifdef WIN32
TEST_CASE("rhi::d3d12::ui_system" * doctest::description("Rendering ImGui UI controls, properties, and tree nodes."))
{
    TestAppDescriptor desc{};
    desc.name           = "d3d12";
    desc.window         = false;
    desc.backend        = RHIBackend::D3D12;
    desc.width          = 960;
    desc.height         = 540;
    desc.rhi_flags      = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_target = CompileTarget::DXIL;
    desc.compile_flags  = CompileFlag::DEBUG;
    UISystemApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::ui_system" * doctest::description("Rendering ImGui UI controls, properties, and tree nodes."))
{
    TestAppDescriptor desc{};
    desc.name           = "metal";
    desc.window         = false;
    desc.backend        = RHIBackend::METAL;
    desc.width          = 960;
    desc.height         = 540;
    desc.rhi_flags      = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_target = CompileTarget::MSL;
    desc.compile_flags  = CompileFlag::DEBUG;
    UISystemApp(desc).run();
}
#endif

TEST_CASE("rhi::softgpu::ui_system" * doctest::description("Rendering ImGui UI controls, properties, and tree nodes."))
{
    TestAppDescriptor desc{};
    desc.name           = "softgpu";
    desc.window         = false;
    desc.backend        = RHIBackend::SOFTGPU;
    desc.width          = 960;
    desc.height         = 540;
    desc.rhi_flags      = RHIFlag::DEBUG;
    desc.compile_target = CompileTarget::SPIRV;
    desc.compile_flags  = CompileFlag::DEBUG;
    UISystemApp(desc).run();
}
