#include <Lyra/Player/EditorLayer.h>

using namespace lyra;

EditorLayer::EditorLayer(const GUIDescriptor& descriptor) : descriptor(descriptor)
{
    gui = GUIRenderer::init(descriptor);

    // NOTE: This call will properly setup ImGuiContext* in the lyra-engine shared library.
    // Users should still call it again in the user application as well for the same reason
    // if they intend to directly call ImGui functions in their executable.
    apply_context();
}

void EditorLayer::bind(Application& app)
{
    // save imgui manager into blackboard
    app.get_blackboard().add<GUIRenderer*>(gui.get());

    // bind imgui manager events
    app.bind<AppEvent::INIT, &EditorLayer::theme>(*this);
    app.bind<AppEvent::RESIZE, &EditorLayer::resize>(*this);
    app.bind<AppEvent::UPDATE, &EditorLayer::update>(*this);
    app.bind<AppEvent::UPDATE_PRE, &EditorLayer::pre_update>(*this);
    app.bind<AppEvent::UPDATE_POST, &EditorLayer::post_update>(*this);
    app.bind<AppEvent::RENDER_POST, &EditorLayer::render>(*this);
}

void EditorLayer::update(Blackboard&)
{
    gui->update();
}

void EditorLayer::pre_update(Blackboard&)
{
    gui->new_frame();
}

void EditorLayer::post_update(Blackboard&)
{
    gui->end_frame();
}

void EditorLayer::render(Blackboard&)
{
    if (descriptor.viewports)
        gui->render_side_viewports();
}

void EditorLayer::resize(Blackboard&)
{
    gui->resize();
}

void EditorLayer::theme(Blackboard&)
{
    ImGuiStyle& style  = ImGui::GetStyle();
    ImVec4*     colors = style.Colors;

    // ui config
    style.WindowPadding.x      = 8.0f;
    style.WindowPadding.y      = 8.0f;
    style.FramePadding.x       = 8.0f;
    style.FramePadding.y       = 6.0f;
    style.ItemSpacing.x        = 8.0f;
    style.ItemSpacing.y        = 6.0f;
    style.ItemInnerSpacing.x   = 6.0f;
    style.ItemInnerSpacing.y   = 6.0f;
    style.GrabMinSize          = 10.0f;
    style.DockingSeparatorSize = 8.0f;
    style.IndentSpacing        = 20.0f;

    // corners
    style.WindowRounding    = 8.0f;
    style.ChildRounding     = 8.0f;
    style.FrameRounding     = 6.0f;
    style.PopupRounding     = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding      = 6.0f;
    style.TabRounding       = 6.0f;

    // windows
    style.WindowMenuButtonPosition = ImGuiDir_Right;

    // base colors for a pleasant and modern dark theme with dark accents
    colors[ImGuiCol_Text]                  = ImVec4(0.92f, 0.93f, 0.94f, 1.00f); // light grey text for readability
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.52f, 0.54f, 1.00f); // subtle grey for disabled text
    colors[ImGuiCol_WindowBg]              = ImVec4(0.14f, 0.14f, 0.16f, 1.00f); // dark background with a hint of blue
    colors[ImGuiCol_ChildBg]               = ImVec4(0.16f, 0.16f, 0.18f, 1.00f); // slightly lighter for child elements
    colors[ImGuiCol_PopupBg]               = ImVec4(0.18f, 0.18f, 0.20f, 1.00f); // popup background
    colors[ImGuiCol_Border]                = ImVec4(0.28f, 0.29f, 0.30f, 0.60f); // soft border color
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f); // no border shadow
    colors[ImGuiCol_FrameBg]               = ImVec4(0.20f, 0.22f, 0.24f, 1.00f); // frame background
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.22f, 0.24f, 0.26f, 1.00f); // frame hover effect
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.24f, 0.26f, 0.28f, 1.00f); // active frame background
    colors[ImGuiCol_TitleBg]               = ImVec4(0.14f, 0.14f, 0.16f, 1.00f); // title background
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.16f, 0.16f, 0.18f, 1.00f); // active title background
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.14f, 0.14f, 0.16f, 1.00f); // collapsed title background
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.20f, 0.20f, 0.22f, 1.00f); // menu bar background
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.16f, 0.16f, 0.18f, 1.00f); // scrollbar background
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.24f, 0.26f, 0.28f, 1.00f); // dark accent for scrollbar grab
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.28f, 0.30f, 0.32f, 1.00f); // scrollbar grab hover
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.32f, 0.34f, 0.36f, 1.00f); // scrollbar grab active
    colors[ImGuiCol_CheckMark]             = ImVec4(0.46f, 0.56f, 0.66f, 1.00f); // dark blue checkmark
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.36f, 0.46f, 0.56f, 1.00f); // dark blue slider grab
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.40f, 0.50f, 0.60f, 1.00f); // active slider grab
    colors[ImGuiCol_Button]                = ImVec4(0.24f, 0.34f, 0.44f, 1.00f); // dark blue button
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.28f, 0.38f, 0.48f, 1.00f); // button hover effect
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.32f, 0.42f, 0.52f, 1.00f); // active button
    colors[ImGuiCol_Header]                = ImVec4(0.24f, 0.34f, 0.44f, 1.00f); // header color similar to button
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.28f, 0.38f, 0.48f, 1.00f); // header hover effect
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.32f, 0.42f, 0.52f, 1.00f); // active header
    colors[ImGuiCol_Separator]             = ImVec4(0.28f, 0.29f, 0.30f, 1.00f); // separator color
    colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.46f, 0.56f, 0.66f, 1.00f); // hover effect for separator
    colors[ImGuiCol_SeparatorActive]       = ImVec4(0.46f, 0.56f, 0.66f, 1.00f); // active separator
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.36f, 0.46f, 0.56f, 1.00f); // resize grip
    colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.40f, 0.50f, 0.60f, 1.00f); // hover effect for resize grip
    colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.44f, 0.54f, 0.64f, 1.00f); // active resize grip
    colors[ImGuiCol_Tab]                   = ImVec4(0.20f, 0.22f, 0.24f, 1.00f); // inactive tab
    colors[ImGuiCol_TabHovered]            = ImVec4(0.28f, 0.38f, 0.48f, 1.00f); // hover effect for tab
    colors[ImGuiCol_TabActive]             = ImVec4(0.24f, 0.34f, 0.44f, 1.00f); // active tab color
    colors[ImGuiCol_TabUnfocused]          = ImVec4(0.20f, 0.22f, 0.24f, 1.00f); // unfocused tab
    colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.24f, 0.34f, 0.44f, 1.00f); // active but unfocused tab
    colors[ImGuiCol_PlotLines]             = ImVec4(0.46f, 0.56f, 0.66f, 1.00f); // plot lines
    colors[ImGuiCol_PlotLinesHovered]      = ImVec4(0.46f, 0.56f, 0.66f, 1.00f); // hover effect for plot lines
    colors[ImGuiCol_PlotHistogram]         = ImVec4(0.36f, 0.46f, 0.56f, 1.00f); // histogram color
    colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(0.40f, 0.50f, 0.60f, 1.00f); // hover effect for histogram
    colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.20f, 0.22f, 0.24f, 1.00f); // table header background
    colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.28f, 0.29f, 0.30f, 1.00f); // strong border for tables
    colors[ImGuiCol_TableBorderLight]      = ImVec4(0.24f, 0.25f, 0.26f, 1.00f); // light border for tables
    colors[ImGuiCol_TableRowBg]            = ImVec4(0.20f, 0.22f, 0.24f, 1.00f); // table row background
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(0.22f, 0.24f, 0.26f, 1.00f); // alternate row background
    colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.24f, 0.34f, 0.44f, 0.35f); // selected text background
    colors[ImGuiCol_DragDropTarget]        = ImVec4(0.46f, 0.56f, 0.66f, 0.90f); // drag and drop target
    colors[ImGuiCol_NavHighlight]          = ImVec4(0.46f, 0.56f, 0.66f, 1.00f); // navigation highlight
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f); // windowing highlight
    colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.80f, 0.80f, 0.80f, 0.20f); // dim background for windowing
    colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.35f); // dim background for modal windows
}
