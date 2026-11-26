#pragma once

#ifndef LYRA_ENGINE_RENDERING_FRAME_GRAPH_MEMORY_H
#define LYRA_ENGINE_RENDERING_FRAME_GRAPH_MEMORY_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Graphics/RHIDescs.h>

namespace lyra
{
    struct ResourceLifetimeInterval
    {
        uint physical_id = ~0u;
        uint start_pass  = ~0u;
        uint end_pass    = 0;
        bool is_active   = false;
    };

    struct MemoryAliasingPlan
    {
        Vector<GPUMemoryHint> resource_hints; // indexed by physical_id
        uint                  max_color_slots = 0;
    };

    class FrameGraphMemoryPlanner
    {
    public:
        // calculate disjoint lifetime intervals and assign optimal alias slots
        static auto compute_plan(Span<const ResourceLifetimeInterval> intervals) -> MemoryAliasingPlan;
    };
} // namespace lyra

#endif // LYRA_ENGINE_RENDERING_FRAME_GRAPH_MEMORY_H
