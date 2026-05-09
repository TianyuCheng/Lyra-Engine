#pragma once

#ifndef LYRA_LYRA_RENDER_FRAME_GRAPH_CONTEXT_H
#define LYRA_LYRA_RENDER_FRAME_GRAPH_CONTEXT_H

#include <Lyra/Render/FrameGraphCommon.h> // IWYU pragma: keep

namespace lyra
{
    struct FrameGraphContext
    {
        GPUDevice        device;
        GPUSurface       surface;
        GPUCommandBuffer cmdlist;
    };

} // namespace lyra

#endif // LYRA_LYRA_RENDER_FRAME_GRAPH_CONTEXT_H
