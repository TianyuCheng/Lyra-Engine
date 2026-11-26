#include <Lyra/UISystem/Widgets/UINodeEditor.h>
#include "ImGui.h"
#include <imgui_node_editor.h>

using namespace lyra;
using namespace lyra::ui;
using namespace lyra::ui::imgui;
using namespace lyra::ui::node;

namespace ed = ax::NodeEditor;

namespace
{
    ed::EditorContext* s_default_ctx = nullptr;
    Vector2            s_panning     = {0.0f, 0.0f};

    auto get_default_context() -> ed::EditorContext*
    {
        if (!s_default_ctx) {
            ed::Config config;
            config.SettingsFile = nullptr;
            s_default_ctx       = ed::CreateEditor(&config);
        }
        return s_default_ctx;
    }

    auto ensure_context() -> ed::EditorContext*
    {
        auto* cur = ed::GetCurrentEditor();
        if (!cur) {
            cur = get_default_context();
            ed::SetCurrentEditor(cur);
        }
        return cur;
    }

    struct FrameEventState
    {
        bool   link_created      = false;
        PinId  created_start_pin = 0;
        PinId  created_end_pin   = 0;
        bool   link_destroyed    = false;
        LinkId destroyed_link    = 0;
    };

    FrameEventState g_frame_event;

    void process_events()
    {
        g_frame_event = {};
        if (ed::BeginCreate()) {
            ed::PinId start, end;
            if (ed::QueryNewLink(&start, &end)) {
                if (ed::AcceptNewItem()) {
                    g_frame_event.link_created      = true;
                    g_frame_event.created_start_pin = static_cast<PinId>(start.Get());
                    g_frame_event.created_end_pin   = static_cast<PinId>(end.Get());
                }
            }
            ed::EndCreate();
        }
        if (ed::BeginDelete()) {
            ed::LinkId l;
            if (ed::QueryDeletedLink(&l)) {
                if (ed::AcceptDeletedItem()) {
                    g_frame_event.link_destroyed = true;
                    g_frame_event.destroyed_link = static_cast<LinkId>(l.Get());
                }
            }
            ed::EndDelete();
        }
    }

    void draw_pin_icon(node::PinShape shape, float size = 10.0f)
    {
        auto* draw_list = ImGui::GetWindowDrawList();
        auto  p         = ImGui::GetCursorScreenPos();
        float half      = size * 0.5f;
        auto  center    = ImVec2(p.x + half, p.y + half);
        auto  color     = ImGui::GetColorU32(ImGuiCol_Text);

        switch (shape) {
            case node::PinShape::Circle:
                draw_list->AddCircle(center, half - 1.0f, color, 12, 1.5f);
                break;
            case node::PinShape::CircleFilled:
                draw_list->AddCircleFilled(center, half, color, 12);
                break;
            case node::PinShape::Triangle:
            {
                ImVec2 p1(center.x + half, center.y);
                ImVec2 p2(center.x - half, center.y - half);
                ImVec2 p3(center.x - half, center.y + half);
                draw_list->AddTriangle(p1, p2, p3, color, 1.5f);
                break;
            }
            case node::PinShape::TriangleFilled:
            {
                ImVec2 p1(center.x + half, center.y);
                ImVec2 p2(center.x - half, center.y - half);
                ImVec2 p3(center.x - half, center.y + half);
                draw_list->AddTriangleFilled(p1, p2, p3, color);
                break;
            }
            case node::PinShape::Quad:
                draw_list->AddRect(p, ImVec2(p.x + size, p.y + size), color, 0.0f, 0, 1.5f);
                break;
            case node::PinShape::QuadFilled:
                draw_list->AddRectFilled(p, ImVec2(p.x + size, p.y + size), color);
                break;
        }
        ImGui::Dummy(ImVec2(size, size));
    }

    auto to_ed_color(node::NodeColorSlot slot) -> ed::StyleColor
    {
        switch (slot) {
            case node::NodeColorSlot::NodeBackground:
            case node::NodeColorSlot::NodeBackgroundHovered:
            case node::NodeColorSlot::NodeBackgroundSelected:
            case node::NodeColorSlot::TitleBar:
            case node::NodeColorSlot::TitleBarHovered:
            case node::NodeColorSlot::TitleBarSelected:
                return ed::StyleColor_NodeBg;
            case node::NodeColorSlot::NodeOutline:
                return ed::StyleColor_NodeBorder;
            case node::NodeColorSlot::Pin:
            case node::NodeColorSlot::PinHovered:
                return ed::StyleColor_PinRect;
            case node::NodeColorSlot::Link:
            case node::NodeColorSlot::LinkHovered:
                return ed::StyleColor_HovLinkBorder;
            case node::NodeColorSlot::LinkSelected:
                return ed::StyleColor_SelLinkBorder;
            case node::NodeColorSlot::GridBackground:
                return ed::StyleColor_Bg;
            case node::NodeColorSlot::GridLine:
            case node::NodeColorSlot::GridLinePrimary:
                return ed::StyleColor_Grid;
        }
        return ed::StyleColor_NodeBg;
    }

    auto to_ed_style_var(node::NodeStyleVar var) -> ed::StyleVar
    {
        switch (var) {
            case node::NodeStyleVar::GridSpacing:
            case node::NodeStyleVar::LinkThickness:
                return ed::StyleVar_LinkStrength;
            case node::NodeStyleVar::NodeCornerRounding:
                return ed::StyleVar_NodeRounding;
            case node::NodeStyleVar::NodePadding:
                return ed::StyleVar_NodePadding;
            case node::NodeStyleVar::NodeBorderThickness:
                return ed::StyleVar_NodeBorderWidth;
            case node::NodeStyleVar::LinkLineSegmentsPerLength:
                return ed::StyleVar_FlowMarkerDistance;
            case node::NodeStyleVar::LinkHoverDistance:
                return ed::StyleVar_HoveredNodeBorderWidth;
            case node::NodeStyleVar::PinCircleRadius:
            case node::NodeStyleVar::PinHoverRadius:
                return ed::StyleVar_PinRadius;
            case node::NodeStyleVar::PinOffset:
                return ed::StyleVar_PinRounding;
        }
        return ed::StyleVar_NodeRounding;
    }
} // namespace

node::NodeEditorContext::NodeEditorContext()
{
    ed::Config config;
    config.SettingsFile = nullptr;
    m_handle            = ed::CreateEditor(&config);
}

node::NodeEditorContext::~NodeEditorContext()
{
    if (m_handle) {
        if (ed::GetCurrentEditor() == m_handle) {
            ed::SetCurrentEditor(nullptr);
        }
        ed::DestroyEditor(static_cast<ed::EditorContext*>(m_handle));
        m_handle = nullptr;
    }
}

node::NodeEditorContext::NodeEditorContext(NodeEditorContext&& other) noexcept
    : m_handle(other.m_handle)
{
    other.m_handle = nullptr;
}

auto node::NodeEditorContext::operator=(NodeEditorContext&& other) noexcept -> NodeEditorContext&
{
    if (this != &other) {
        if (m_handle) {
            if (ed::GetCurrentEditor() == m_handle) {
                ed::SetCurrentEditor(nullptr);
            }
            ed::DestroyEditor(static_cast<ed::EditorContext*>(m_handle));
        }
        m_handle       = other.m_handle;
        other.m_handle = nullptr;
    }
    return *this;
}

void node::editor(CString id, ActionRef content)
{
    ed::SetCurrentEditor(get_default_context());
    ed::Begin(id);
    content();
    process_events();
    ed::End();
}

void node::editor(NodeEditorContext& context, ActionRef content)
{
    auto* prev = ed::GetCurrentEditor();
    ed::SetCurrentEditor(static_cast<ed::EditorContext*>(context.native_handle()));
    ed::Begin("editor");
    content();
    process_events();
    ed::End();
    if (prev) {
        ed::SetCurrentEditor(prev);
    }
}

void node::node(NodeId id, ActionRef content)
{
    ed::BeginNode(ed::NodeId(static_cast<uintptr_t>(id)));
    content();
    ed::EndNode();
}

void node::node_title(ActionRef content)
{
    content();
    ImGui::Spacing();
}

void node::input_pin(PinId id, PinShape shape, ActionRef content)
{
    ed::BeginPin(ed::PinId(static_cast<uintptr_t>(id)), ed::PinKind::Input);
    draw_pin_icon(shape);
    ImGui::SameLine();
    content();
    ed::EndPin();
}

void node::input_pin(PinId id, ActionRef content)
{
    input_pin(id, PinShape::CircleFilled, content);
}

void node::output_pin(PinId id, PinShape shape, ActionRef content)
{
    ed::BeginPin(ed::PinId(static_cast<uintptr_t>(id)), ed::PinKind::Output);
    content();
    ImGui::SameLine();
    draw_pin_icon(shape);
    ed::EndPin();
}

void node::output_pin(PinId id, ActionRef content)
{
    output_pin(id, PinShape::CircleFilled, content);
}

void node::static_attribute(PinId /*id*/, ActionRef content)
{
    content();
}

void node::link(LinkId id, PinId start_pin, PinId end_pin)
{
    ed::Link(ed::LinkId(static_cast<uintptr_t>(id)),
        ed::PinId(static_cast<uintptr_t>(start_pin)),
        ed::PinId(static_cast<uintptr_t>(end_pin)));
}

void node::flow(LinkId id)
{
    ed::Flow(ed::LinkId(static_cast<uintptr_t>(id)));
}

void node::group(NodeId id, CString title, Vector2 size, ActionRef content)
{
    ed::BeginNode(ed::NodeId(static_cast<uintptr_t>(id)));
    if (title && title[0] != '\0') {
        ImGui::TextUnformatted(title);
    }
    ed::Group(to_imvec2(size));
    content();
    ed::EndNode();
}

void node::minimap(float /*size_fraction*/, MinimapLocation /*location*/)
{
}

void node::push_color(NodeColorSlot slot, const Vector4& color)
{
    ed::PushStyleColor(to_ed_color(slot), to_imvec4(color));
}

void node::pop_color(uint32_t count)
{
    ed::PopStyleColor(static_cast<int>(count));
}

void node::push_style(NodeStyleVar var, float value)
{
    ed::PushStyleVar(to_ed_style_var(var), value);
}

void node::push_style(NodeStyleVar var, const Vector2& value)
{
    ed::PushStyleVar(to_ed_style_var(var), to_imvec2(value));
}

void node::pop_style(uint32_t count)
{
    ed::PopStyleVar(static_cast<int>(count));
}

bool node::is_link_created(PinId* out_start_pin, PinId* out_end_pin, bool* out_created_from_snap)
{
    if (g_frame_event.link_created) {
        if (out_start_pin) *out_start_pin = g_frame_event.created_start_pin;
        if (out_end_pin) *out_end_pin = g_frame_event.created_end_pin;
        if (out_created_from_snap) *out_created_from_snap = false;
        return true;
    }
    return false;
}

bool node::is_link_created(NodeId* out_start_node, PinId* out_start_pin, NodeId* out_end_node, PinId* out_end_pin, bool* out_created_from_snap)
{
    return is_link_created(out_start_pin, out_end_pin, out_created_from_snap);
}

bool node::is_link_destroyed(LinkId* out_link_id)
{
    if (g_frame_event.link_destroyed) {
        if (out_link_id) *out_link_id = g_frame_event.destroyed_link;
        return true;
    }
    return false;
}

bool node::is_link_dropped(PinId* /*out_started_pin*/, bool /*including_detached*/)
{
    return false;
}

bool node::is_node_hovered(NodeId* out_node_id)
{
    auto h = ed::GetHoveredNode();
    if (h) {
        if (out_node_id) *out_node_id = static_cast<NodeId>(h.Get());
        return true;
    }
    return false;
}

bool node::is_pin_hovered(PinId* out_pin_id)
{
    auto h = ed::GetHoveredPin();
    if (h) {
        if (out_pin_id) *out_pin_id = static_cast<PinId>(h.Get());
        return true;
    }
    return false;
}

bool node::is_link_hovered(LinkId* out_link_id)
{
    auto h = ed::GetHoveredLink();
    if (h) {
        if (out_link_id) *out_link_id = static_cast<LinkId>(h.Get());
        return true;
    }
    return false;
}

bool node::is_editor_hovered()
{
    ensure_context();
    return ed::IsActive();
}

auto node::num_selected_nodes() -> int32_t
{
    ensure_context();
    return ed::GetSelectedNodes(nullptr, 0);
}

auto node::num_selected_links() -> int32_t
{
    ensure_context();
    return ed::GetSelectedLinks(nullptr, 0);
}

void node::get_selected_nodes(Span<NodeId> out_nodes)
{
    if (out_nodes.empty()) return;
    ensure_context();
    Vector<ed::NodeId> temp(out_nodes.size());
    int                count = ed::GetSelectedNodes(temp.data(), static_cast<int>(temp.size()));
    for (int i = 0; i < count && i < static_cast<int>(out_nodes.size()); ++i) {
        out_nodes[i] = static_cast<NodeId>(temp[i].Get());
    }
}

void node::get_selected_links(Span<LinkId> out_links)
{
    if (out_links.empty()) return;
    ensure_context();
    Vector<ed::LinkId> temp(out_links.size());
    int                count = ed::GetSelectedLinks(temp.data(), static_cast<int>(temp.size()));
    for (int i = 0; i < count && i < static_cast<int>(out_links.size()); ++i) {
        out_links[i] = static_cast<LinkId>(temp[i].Get());
    }
}

void node::select_node(NodeId id)
{
    ensure_context();
    ed::SelectNode(ed::NodeId(static_cast<uintptr_t>(id)));
}

void node::clear_node_selection(NodeId id)
{
    ensure_context();
    ed::DeselectNode(ed::NodeId(static_cast<uintptr_t>(id)));
}

void node::clear_selection()
{
    ensure_context();
    ed::ClearSelection();
}

bool node::is_node_selected(NodeId id)
{
    ensure_context();
    return ed::IsNodeSelected(ed::NodeId(static_cast<uintptr_t>(id)));
}

bool node::is_link_selected(LinkId id)
{
    ensure_context();
    return ed::IsLinkSelected(ed::LinkId(static_cast<uintptr_t>(id)));
}

auto node::get_node_position(NodeId id, CoordinateSpace space) -> Vector2
{
    ensure_context();
    auto pos = ed::GetNodePosition(ed::NodeId(static_cast<uintptr_t>(id)));
    if (space == CoordinateSpace::Screen) {
        pos = ed::CanvasToScreen(pos);
    }
    return to_vector2(pos);
}

void node::set_node_position(NodeId id, const Vector2& pos, CoordinateSpace space)
{
    ensure_context();
    auto p = to_imvec2(pos);
    if (space == CoordinateSpace::Screen) {
        p = ed::ScreenToCanvas(p);
    }
    ed::SetNodePosition(ed::NodeId(static_cast<uintptr_t>(id)), p);
}

auto node::get_node_dimensions(NodeId id) -> Vector2
{
    ensure_context();
    return to_vector2(ed::GetNodeSize(ed::NodeId(static_cast<uintptr_t>(id))));
}

void node::snap_node_to_grid(NodeId /*id*/)
{
}

void node::set_node_draggable(NodeId /*id*/, bool /*draggable*/)
{
}

auto node::get_panning() -> Vector2
{
    ensure_context();
    return s_panning;
}

void node::reset_panning(const Vector2& pos)
{
    ensure_context();
    s_panning = pos;
}

void node::move_to_node(NodeId id)
{
    ensure_context();
    ed::CenterNodeOnScreen(ed::NodeId(static_cast<uintptr_t>(id)));
}

auto node::get_zoom() -> float
{
    ensure_context();
    return ed::GetCurrentZoom();
}

void node::navigate_to_content(float duration)
{
    ensure_context();
    ed::NavigateToContent(duration);
}

void node::navigate_to_selection(bool zoom_in, float duration)
{
    ensure_context();
    ed::NavigateToSelection(zoom_in, duration);
}

auto node::save_editor_state(const NodeEditorContext* /*ctx*/) -> String
{
    return "";
}

void node::load_editor_state(StringView /*ini_data*/, NodeEditorContext* /*ctx*/)
{
}
