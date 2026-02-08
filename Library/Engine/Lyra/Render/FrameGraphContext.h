#pragma once

#ifndef LYRA_LIBRARY_ENGINE_RENDER_FRAME_GRAPH_CONTEXT_H
#define LYRA_LIBRARY_ENGINE_RENDER_FRAME_GRAPH_CONTEXT_H

#include "FrameGraphCommon.h" // IWYU pragma: keep

namespace lyra
{
    struct FrameGraphContext
    {
        GPUDevice        device;
        GPUSurface       surface;
        GPUCommandBuffer cmdlist;
    };

} // namespace lyra

#endif // LYRA_LIBRARY_ENGINE_RENDER_FRAME_GRAPH_CONTEXT_H
