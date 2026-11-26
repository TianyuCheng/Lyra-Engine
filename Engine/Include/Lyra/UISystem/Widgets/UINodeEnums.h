#pragma once

#ifndef LYRA_ENGINE_UISYSTEM_WIDGETS_UINODEENUMS_H
#define LYRA_ENGINE_UISYSTEM_WIDGETS_UINODEENUMS_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/BitFlags.h>

namespace lyra::ui::node
{
    using NodeId = int32_t;
    using PinId  = int32_t;
    using LinkId = int32_t;

    /**
     * @brief visual pin shape displayed at node input and output ports.
     */
    enum struct PinShape : uint8_t
    {
        Circle,
        CircleFilled,
        Triangle,
        TriangleFilled,
        Quad,
        QuadFilled,
    };

    /**
     * @brief port type for node attributes.
     */
    enum struct PinKind : uint8_t
    {
        Input,
        Output,
        Static,
    };

    /**
     * @brief coordinate space systems for querying and placing nodes.
     */
    enum struct CoordinateSpace : uint8_t
    {
        Grid,   // canvas grid coordinates accounting for editor panning
        Editor, // local to top-left of the node editor window
        Screen, // global window screen coordinates
    };

    /**
     * @brief corner placement of the navigation minimap.
     */
    enum struct MinimapLocation : uint8_t
    {
        BottomLeft,
        BottomRight,
        TopLeft,
        TopRight,
    };

    /**
     * @brief styling color slots for node elements.
     */
    enum struct NodeColorSlot : uint8_t
    {
        NodeBackground,
        NodeBackgroundHovered,
        NodeBackgroundSelected,
        NodeOutline,
        TitleBar,
        TitleBarHovered,
        TitleBarSelected,
        Pin,
        PinHovered,
        Link,
        LinkHovered,
        LinkSelected,
        GridBackground,
        GridLine,
        GridLinePrimary,
    };

    /**
     * @brief numerical and metric style properties.
     */
    enum struct NodeStyleVar : uint8_t
    {
        GridSpacing,
        NodeCornerRounding,
        NodePadding,
        NodeBorderThickness,
        LinkThickness,
        LinkLineSegmentsPerLength,
        LinkHoverDistance,
        PinCircleRadius,
        PinHoverRadius,
        PinOffset,
    };

    /**
     * @brief node editor canvas behavior flags.
     */
    enum struct NodeEditorFlag : uint32_t
    {
        None               = 0,
        EnableGridLines    = 1 << 0,
        EnableGridSnapping = 1 << 1,
        EnableNodeOutline  = 1 << 2,
    };

} // namespace lyra::ui::node

ENABLE_BIT_FLAGS(lyra::ui::node::NodeEditorFlag);

#endif // LYRA_ENGINE_UISYSTEM_WIDGETS_UINODEENUMS_H
