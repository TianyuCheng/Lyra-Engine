#pragma once

#ifndef LYRA_LIBRARY_FRAME_GRAPH_ENUMS_H
#define LYRA_LIBRARY_FRAME_GRAPH_ENUMS_H

namespace lyra
{
    enum struct FrameGraphResourceType
    {
        TRANSIENT,
        IMPORTED,
    };

    enum struct FrameGraphReadOp
    {
        NOP, // no specific action required
        READ,
        SAMPLE,
        PRESENT,
    };

    enum struct FrameGraphWriteOp
    {
        NOP, // no specific action required
        WRITE,
        RENDER,
    };

    using FGReadOp       = FrameGraphReadOp;
    using FGWriteOp      = FrameGraphWriteOp;
    using FGResourceType = FrameGraphResourceType;

} // namespace lyra

#endif // LYRA_LIBRARY_FRAME_GRAPH_ENUMS_H
