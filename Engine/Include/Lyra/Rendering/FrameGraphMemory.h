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
        Vector<GPUMemoryHint> resource_hints;          // indexed by physical_id
        Vector<bool>          has_aliased_predecessor; // true if this resource replaces a previous occupant on its slot
        Vector<uint>          predecessor_physical_id; // physical_id of previous occupant, or ~0u
        uint                  max_color_slots = 0;
    };

    struct FrameGraphMemoryPlanner
    {
    public:
        // calculate disjoint lifetime intervals and assign optimal alias slots
        static auto compute_plan(View<ResourceLifetimeInterval> intervals) -> MemoryAliasingPlan;
    };
} // namespace lyra

#endif // LYRA_ENGINE_RENDERING_FRAME_GRAPH_MEMORY_H
