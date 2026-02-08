#pragma once

#ifndef LYRA_LIBRARY_ENGINE_RENDER_FRAME_GRAPH_H
#define LYRA_LIBRARY_ENGINE_RENDER_FRAME_GRAPH_H

#include "FrameGraphCommon.h" // IWYU pragma: keep
#include "FrameGraphPass.h"
#include "FrameGraphContext.h"
#include "FrameGraphAllocator.h"
#include "FrameGraphResource.h"
#include "FrameGraphTexture.h"
#include "FrameGraphBuffer.h"

namespace lyra
{
    struct FrameGraphBuilder;

    struct FrameGraph
    {
    public:
        friend struct FrameGraphBuilder;

        using Pass      = FrameGraphPass;
        using Builder   = FrameGraphBuilder;
        using Context   = FrameGraphContext;
        using Resource  = FrameGraphResource;
        using Resources = FrameGraphResources;
        using Buffer    = FrameGraphBuffer;
        using Texture   = FrameGraphTexture;
        using Allocator = FrameGraphAllocator;

        explicit FrameGraph()                   = default;
        explicit FrameGraph(FrameGraph&&)       = delete;
        explicit FrameGraph(const FrameGraph&&) = delete;
        virtual ~FrameGraph();

        void execute(FrameGraphContext* context, FrameGraphAllocator* allocator);

    private:
        void compile();

        bool has_cycles() const;
        bool has_cycles(HashSet<uint>& visited_passes, HashSet<uint>& recursion_set, uint psid) const;

        template <typename T>
        void for_all_consumers(const FrameGraphResourceNode& resource, T&& callback)
        {
            for (auto& pass : resource.consumers)
                callback(passes.at(pass));
        }

        template <typename T>
        void for_all_producers(const FrameGraphResourceNode& resource, T&& callback)
        {
            for (auto& pass : resource.producers)
                callback(passes.at(pass));
        }

        template <typename T>
        void for_all_reads(const FrameGraphPassNode& pass, T&& callback)
        {
            for (auto& resource : pass.reads)
                callback(resource);
        }

        template <typename T>
        void for_all_writes(const FrameGraphPassNode& pass, T&& callback)
        {
            for (auto& resource : pass.writes)
                callback(resource);
        }

    private:
        Vector<FrameGraphPassNode>     passes;
        Vector<FrameGraphResourceNode> resources;
        FrameGraphResources            registry;
    }; // end of FrameGraph

} // namespace lyra

#endif // LYRA_LIBRARY_ENGINE_RENDER_FRAME_GRAPH_H
