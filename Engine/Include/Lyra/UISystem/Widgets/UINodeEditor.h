#pragma once

#ifndef LYRA_ENGINE_UISYSTEM_WIDGETS_UINODEEDITOR_H
#define LYRA_ENGINE_UISYSTEM_WIDGETS_UINODEEDITOR_H

#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/UISystem/Widgets/UI.h>
#include <Lyra/UISystem/Widgets/UINodeEnums.h>

namespace lyra::ui::node
{
    using EditorFlags = BitFlags<NodeEditorFlag>;

    /**
     * @brief opaque editor context supporting multiple independent graph canvases (e.g. tabs).
     */
    class NodeEditorContext
    {
    public:
        NodeEditorContext();
        ~NodeEditorContext();

        NodeEditorContext(const NodeEditorContext&) = delete;
        auto operator=(const NodeEditorContext&) -> NodeEditorContext& = delete;

        NodeEditorContext(NodeEditorContext&& other) noexcept;
        auto operator=(NodeEditorContext&& other) noexcept -> NodeEditorContext&;

        [[nodiscard]] auto native_handle() const -> void* { return m_handle; }

    private:
        void* m_handle = nullptr;
    };

    // =========================================================================
    // 1. Editor Canvas Lifecycle
    // =========================================================================

    /**
     * @brief renders the active node editor workspace using the default context.
     */
    void editor(CString id, ActionRef content);

    /**
     * @brief renders the node editor workspace bound to an explicit context (for multi-graph workflows).
     */
    void editor(NodeEditorContext& context, ActionRef content);

    // =========================================================================
    // 2. Node Hierarchy & Attributes
    // =========================================================================

    /**
     * @brief declares a node scope.
     */
    void node(NodeId id, ActionRef content);

    /**
     * @brief declares the header / title bar area of the current node.
     */
    void node_title(ActionRef content);

    /**
     * @brief declares an input pin (rendered on the left edge).
     */
    void input_pin(PinId id, PinShape shape, ActionRef content);
    void input_pin(PinId id, ActionRef content);

    /**
     * @brief declares an output pin (rendered on the right edge).
     */
    void output_pin(PinId id, PinShape shape, ActionRef content);
    void output_pin(PinId id, ActionRef content);

    /**
     * @brief declares embedded static content without pins (e.g. embedded sliders, preview images).
     */
    void static_attribute(PinId id, ActionRef content);

    // =========================================================================
    // 3. Links & Auxiliary Widgets
    // =========================================================================

    /**
     * @brief renders a directional connection between two pins.
     */
    void link(LinkId id, PinId start_pin, PinId end_pin);

    /**
     * @brief displays an interactive canvas minimap overlay.
     */
    void minimap(float size_fraction = 0.2f, MinimapLocation location = MinimapLocation::BottomRight);

    // =========================================================================
    // 4. Styling & Customization
    // =========================================================================

    void push_color(NodeColorSlot slot, const Vector4& color);
    void pop_color(uint32_t count = 1);

    void push_style(NodeStyleVar var, float value);
    void push_style(NodeStyleVar var, const Vector2& value);
    void pop_style(uint32_t count = 1);

    // =========================================================================
    // 5. Interaction & Event Queries (Call after editor() or within same frame)
    // =========================================================================

    /**
     * @brief returns true if a new link connection was just completed this frame.
     */
    bool is_link_created(PinId* out_start_pin, PinId* out_end_pin, bool* out_created_from_snap = nullptr);
    bool is_link_created(NodeId* out_start_node, PinId* out_start_pin, NodeId* out_end_node, PinId* out_end_pin, bool* out_created_from_snap = nullptr);

    /**
     * @brief returns true if an existing link was disconnected / detached by the user.
     */
    bool is_link_destroyed(LinkId* out_link_id);

    /**
     * @brief returns true if a dragged link was dropped in empty space without connection.
     */
    bool is_link_dropped(PinId* out_started_pin = nullptr, bool including_detached = true);

    /**
     * @brief hover inspection queries.
     */
    bool is_node_hovered(NodeId* out_node_id);
    bool is_pin_hovered(PinId* out_pin_id);
    bool is_link_hovered(LinkId* out_link_id);
    bool is_editor_hovered();

    /**
     * @brief selection management.
     */
    auto num_selected_nodes() -> int32_t;
    auto num_selected_links() -> int32_t;
    void get_selected_nodes(Span<NodeId> out_nodes);
    void get_selected_links(Span<LinkId> out_links);
    void select_node(NodeId id);
    void clear_node_selection(NodeId id);
    void clear_selection();
    bool is_node_selected(NodeId id);
    bool is_link_selected(LinkId id);

    // =========================================================================
    // 6. Navigation & Positioning
    // =========================================================================

    auto get_node_position(NodeId id, CoordinateSpace space = CoordinateSpace::Grid) -> Vector2;
    void set_node_position(NodeId id, const Vector2& pos, CoordinateSpace space = CoordinateSpace::Grid);
    auto get_node_dimensions(NodeId id) -> Vector2;
    void snap_node_to_grid(NodeId id);
    void set_node_draggable(NodeId id, bool draggable);

    auto get_panning() -> Vector2;
    void reset_panning(const Vector2& pos = {0.0f, 0.0f});
    void move_to_node(NodeId id);

    // =========================================================================
    // 7. Serialization / State Persistence
    // =========================================================================

    auto save_editor_state(const NodeEditorContext* ctx = nullptr) -> String;
    void load_editor_state(StringView ini_data, NodeEditorContext* ctx = nullptr);

} // namespace lyra::ui::node

#endif // LYRA_ENGINE_UISYSTEM_WIDGETS_UINODEEDITOR_H
