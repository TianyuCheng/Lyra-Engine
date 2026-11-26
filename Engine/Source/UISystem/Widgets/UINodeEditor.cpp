#include <Lyra/UISystem/Widgets/UINodeEditor.h>
#include "ImGui.h"
#include <imnodes.h>
#include <imnodes_internal.h>

using namespace lyra;
using namespace lyra::ui;
using namespace lyra::ui::imgui;

namespace
{
    void ensure_context()
    {
        if (ImNodes::GetCurrentContext() == nullptr) {
            ImNodes::CreateContext();
            ImNodes::StyleColorsDark();
        }
    }

    auto to_imnodes_pin_shape(node::PinShape shape) -> ImNodesPinShape
    {
        switch (shape) {
            case node::PinShape::Circle:         return ImNodesPinShape_Circle;
            case node::PinShape::CircleFilled:   return ImNodesPinShape_CircleFilled;
            case node::PinShape::Triangle:       return ImNodesPinShape_Triangle;
            case node::PinShape::TriangleFilled: return ImNodesPinShape_TriangleFilled;
            case node::PinShape::Quad:           return ImNodesPinShape_Quad;
            case node::PinShape::QuadFilled:     return ImNodesPinShape_QuadFilled;
        }
        return ImNodesPinShape_CircleFilled;
    }

    auto to_imnodes_minimap_location(node::MinimapLocation location) -> ImNodesMiniMapLocation
    {
        switch (location) {
            case node::MinimapLocation::BottomLeft:  return ImNodesMiniMapLocation_BottomLeft;
            case node::MinimapLocation::BottomRight: return ImNodesMiniMapLocation_BottomRight;
            case node::MinimapLocation::TopLeft:     return ImNodesMiniMapLocation_TopLeft;
            case node::MinimapLocation::TopRight:    return ImNodesMiniMapLocation_TopRight;
        }
        return ImNodesMiniMapLocation_BottomRight;
    }

    auto to_imnodes_col(node::NodeColorSlot slot) -> ImNodesCol
    {
        switch (slot) {
            case node::NodeColorSlot::NodeBackground:         return ImNodesCol_NodeBackground;
            case node::NodeColorSlot::NodeBackgroundHovered:  return ImNodesCol_NodeBackgroundHovered;
            case node::NodeColorSlot::NodeBackgroundSelected: return ImNodesCol_NodeBackgroundSelected;
            case node::NodeColorSlot::NodeOutline:            return ImNodesCol_NodeOutline;
            case node::NodeColorSlot::TitleBar:               return ImNodesCol_TitleBar;
            case node::NodeColorSlot::TitleBarHovered:        return ImNodesCol_TitleBarHovered;
            case node::NodeColorSlot::TitleBarSelected:       return ImNodesCol_TitleBarSelected;
            case node::NodeColorSlot::Pin:                    return ImNodesCol_Pin;
            case node::NodeColorSlot::PinHovered:             return ImNodesCol_PinHovered;
            case node::NodeColorSlot::Link:                   return ImNodesCol_Link;
            case node::NodeColorSlot::LinkHovered:            return ImNodesCol_LinkHovered;
            case node::NodeColorSlot::LinkSelected:           return ImNodesCol_LinkSelected;
            case node::NodeColorSlot::GridBackground:         return ImNodesCol_GridBackground;
            case node::NodeColorSlot::GridLine:               return ImNodesCol_GridLine;
            case node::NodeColorSlot::GridLinePrimary:        return ImNodesCol_GridLinePrimary;
        }
        return ImNodesCol_NodeBackground;
    }

    auto to_imnodes_style_var(node::NodeStyleVar var) -> ImNodesStyleVar
    {
        switch (var) {
            case node::NodeStyleVar::GridSpacing:                return ImNodesStyleVar_GridSpacing;
            case node::NodeStyleVar::NodeCornerRounding:         return ImNodesStyleVar_NodeCornerRounding;
            case node::NodeStyleVar::NodePadding:                return ImNodesStyleVar_NodePadding;
            case node::NodeStyleVar::NodeBorderThickness:        return ImNodesStyleVar_NodeBorderThickness;
            case node::NodeStyleVar::LinkThickness:              return ImNodesStyleVar_LinkThickness;
            case node::NodeStyleVar::LinkLineSegmentsPerLength:  return ImNodesStyleVar_LinkLineSegmentsPerLength;
            case node::NodeStyleVar::LinkHoverDistance:          return ImNodesStyleVar_LinkHoverDistance;
            case node::NodeStyleVar::PinCircleRadius:            return ImNodesStyleVar_PinCircleRadius;
            case node::NodeStyleVar::PinHoverRadius:             return ImNodesStyleVar_PinHoverRadius;
            case node::NodeStyleVar::PinOffset:                  return ImNodesStyleVar_PinOffset;
        }
        return ImNodesStyleVar_GridSpacing;
    }
} // namespace

node::NodeEditorContext::NodeEditorContext()
{
    ensure_context();
    m_handle = ImNodes::EditorContextCreate();
}

node::NodeEditorContext::~NodeEditorContext()
{
    if (m_handle) {
        if (GImNodes && GImNodes->EditorCtx == m_handle) {
            ImNodes::EditorContextSet(GImNodes->DefaultEditorCtx);
        }
        ImNodes::EditorContextFree(static_cast<ImNodesEditorContext*>(m_handle));
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
            if (GImNodes && GImNodes->EditorCtx == m_handle) {
                ImNodes::EditorContextSet(GImNodes->DefaultEditorCtx);
            }
            ImNodes::EditorContextFree(static_cast<ImNodesEditorContext*>(m_handle));
        }
        m_handle       = other.m_handle;
        other.m_handle = nullptr;
    }
    return *this;
}

void node::editor(CString id, ActionRef content)
{
    ensure_context();
    ImGui::PushID(id);
    ImNodes::BeginNodeEditor();
    content();
    ImNodes::EndNodeEditor();
    ImGui::PopID();
}

void node::editor(NodeEditorContext& context, ActionRef content)
{
    ensure_context();
    auto* prev_editor = GImNodes ? GImNodes->EditorCtx : nullptr;
    ImNodes::EditorContextSet(static_cast<ImNodesEditorContext*>(context.native_handle()));
    ImNodes::BeginNodeEditor();
    content();
    ImNodes::EndNodeEditor();
    if (prev_editor) {
        ImNodes::EditorContextSet(prev_editor);
    }
}

void node::node(NodeId id, ActionRef content)
{
    ImNodes::BeginNode(id);
    content();
    ImNodes::EndNode();
}

void node::node_title(ActionRef content)
{
    ImNodes::BeginNodeTitleBar();
    content();
    ImNodes::EndNodeTitleBar();
}

void node::input_pin(PinId id, PinShape shape, ActionRef content)
{
    ImNodes::BeginInputAttribute(id, to_imnodes_pin_shape(shape));
    content();
    ImNodes::EndInputAttribute();
}

void node::input_pin(PinId id, ActionRef content)
{
    input_pin(id, PinShape::CircleFilled, content);
}

void node::output_pin(PinId id, PinShape shape, ActionRef content)
{
    ImNodes::BeginOutputAttribute(id, to_imnodes_pin_shape(shape));
    content();
    ImNodes::EndOutputAttribute();
}

void node::output_pin(PinId id, ActionRef content)
{
    output_pin(id, PinShape::CircleFilled, content);
}

void node::static_attribute(PinId id, ActionRef content)
{
    ImNodes::BeginStaticAttribute(id);
    content();
    ImNodes::EndStaticAttribute();
}

void node::link(LinkId id, PinId start_pin, PinId end_pin)
{
    ImNodes::Link(id, start_pin, end_pin);
}

void node::minimap(float size_fraction, MinimapLocation location)
{
    ImNodes::MiniMap(size_fraction, to_imnodes_minimap_location(location));
}

void node::push_color(NodeColorSlot slot, const Vector4& color)
{
    ImNodes::PushColorStyle(to_imnodes_col(slot), ImGui::ColorConvertFloat4ToU32(to_imvec4(color)));
}

void node::pop_color(uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i) {
        ImNodes::PopColorStyle();
    }
}

void node::push_style(NodeStyleVar var, float value)
{
    ImNodes::PushStyleVar(to_imnodes_style_var(var), value);
}

void node::push_style(NodeStyleVar var, const Vector2& value)
{
    ImNodes::PushStyleVar(to_imnodes_style_var(var), to_imvec2(value));
}

void node::pop_style(uint32_t count)
{
    ImNodes::PopStyleVar(static_cast<int>(count));
}

bool node::is_link_created(PinId* out_start_pin, PinId* out_end_pin, bool* out_created_from_snap)
{
    return ImNodes::IsLinkCreated(out_start_pin, out_end_pin, out_created_from_snap);
}

bool node::is_link_created(NodeId* out_start_node, PinId* out_start_pin, NodeId* out_end_node, PinId* out_end_pin, bool* out_created_from_snap)
{
    return ImNodes::IsLinkCreated(out_start_node, out_start_pin, out_end_node, out_end_pin, out_created_from_snap);
}

bool node::is_link_destroyed(LinkId* out_link_id)
{
    return ImNodes::IsLinkDestroyed(out_link_id);
}

bool node::is_link_dropped(PinId* out_started_pin, bool including_detached)
{
    return ImNodes::IsLinkDropped(out_started_pin, including_detached);
}

bool node::is_node_hovered(NodeId* out_node_id)
{
    return ImNodes::IsNodeHovered(out_node_id);
}

bool node::is_pin_hovered(PinId* out_pin_id)
{
    return ImNodes::IsPinHovered(out_pin_id);
}

bool node::is_link_hovered(LinkId* out_link_id)
{
    return ImNodes::IsLinkHovered(out_link_id);
}

bool node::is_editor_hovered()
{
    return ImNodes::IsEditorHovered();
}

auto node::num_selected_nodes() -> int32_t
{
    return ImNodes::NumSelectedNodes();
}

auto node::num_selected_links() -> int32_t
{
    return ImNodes::NumSelectedLinks();
}

void node::get_selected_nodes(Span<NodeId> out_nodes)
{
    if (!out_nodes.empty()) {
        ImNodes::GetSelectedNodes(out_nodes.data());
    }
}

void node::get_selected_links(Span<LinkId> out_links)
{
    if (!out_links.empty()) {
        ImNodes::GetSelectedLinks(out_links.data());
    }
}

void node::select_node(NodeId id)
{
    ImNodes::SelectNode(id);
}

void node::clear_node_selection(NodeId id)
{
    ImNodes::ClearNodeSelection(id);
}

void node::clear_selection()
{
    ImNodes::ClearNodeSelection();
    ImNodes::ClearLinkSelection();
}

bool node::is_node_selected(NodeId id)
{
    return ImNodes::IsNodeSelected(id);
}

bool node::is_link_selected(LinkId id)
{
    return ImNodes::IsLinkSelected(id);
}

auto node::get_node_position(NodeId id, CoordinateSpace space) -> Vector2
{
    switch (space) {
        case CoordinateSpace::Grid:   return to_vector2(ImNodes::GetNodeGridSpacePos(id));
        case CoordinateSpace::Editor: return to_vector2(ImNodes::GetNodeEditorSpacePos(id));
        case CoordinateSpace::Screen: return to_vector2(ImNodes::GetNodeScreenSpacePos(id));
    }
    return Vector2(0.0f);
}

void node::set_node_position(NodeId id, const Vector2& pos, CoordinateSpace space)
{
    switch (space) {
        case CoordinateSpace::Grid:   ImNodes::SetNodeGridSpacePos(id, to_imvec2(pos)); break;
        case CoordinateSpace::Editor: ImNodes::SetNodeEditorSpacePos(id, to_imvec2(pos)); break;
        case CoordinateSpace::Screen: ImNodes::SetNodeScreenSpacePos(id, to_imvec2(pos)); break;
    }
}

auto node::get_node_dimensions(NodeId id) -> Vector2
{
    return to_vector2(ImNodes::GetNodeDimensions(id));
}

void node::snap_node_to_grid(NodeId id)
{
    ImNodes::SnapNodeToGrid(id);
}

void node::set_node_draggable(NodeId id, bool draggable)
{
    ImNodes::SetNodeDraggable(id, draggable);
}

auto node::get_panning() -> Vector2
{
    return to_vector2(ImNodes::EditorContextGetPanning());
}

void node::reset_panning(const Vector2& pos)
{
    ImNodes::EditorContextResetPanning(to_imvec2(pos));
}

void node::move_to_node(NodeId id)
{
    ImNodes::EditorContextMoveToNode(id);
}

auto node::save_editor_state(const NodeEditorContext* ctx) -> String
{
    if (ctx) {
        auto* str = ImNodes::SaveEditorStateToIniString(static_cast<const ImNodesEditorContext*>(ctx->native_handle()));
        return str ? String(str) : String();
    }
    auto* str = ImNodes::SaveCurrentEditorStateToIniString();
    return str ? String(str) : String();
}

void node::load_editor_state(StringView ini_data, NodeEditorContext* ctx)
{
    if (ctx) {
        ImNodes::LoadEditorStateFromIniString(static_cast<ImNodesEditorContext*>(ctx->native_handle()), ini_data.data(), ini_data.size());
    } else {
        ImNodes::LoadCurrentEditorStateFromIniString(ini_data.data(), ini_data.size());
    }
}
