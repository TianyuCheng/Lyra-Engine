#pragma once

#ifndef LYRA_LIBRARY_RENDER_FRAME_GRAPH_BUILDER_H
#define LYRA_LIBRARY_RENDER_FRAME_GRAPH_BUILDER_H

#include "FrameGraphCommon.h" // IWYU pragma: keep
#include "FrameGraph.h"
#include "FrameGraphEnums.h"
#include "FrameGraphResource.h"

namespace lyra
{
    struct FrameGraph;

    struct FrameGraphBuilder
    {
    public:
        explicit FrameGraphBuilder() { graph = std::make_unique<FrameGraph>(); }
        explicit FrameGraphBuilder(FrameGraphBuilder&&)      = delete;
        explicit FrameGraphBuilder(const FrameGraphBuilder&) = delete;
        virtual ~FrameGraphBuilder()                         = default;

        FrameGraphBuilder&  operator=(const FrameGraphBuilder&) = delete;
        FrameGraphBuilder&& operator=(FrameGraphBuilder&&)      = delete;

        template <typename T>
        [[nodiscard]] FrameGraphResource import(const T& entry)
        {
            uint index           = static_cast<uint>(graph->resources.size());
            auto resource        = FrameGraphResourceNode{};
            resource.rsid        = index;
            resource.entry       = new FrameGraphResourceEntry<T>{};
            resource.entry->type = FrameGraphResourceType::IMPORTED;

            // imported resources directly stores the resource entry
            reinterpret_cast<FrameGraphResourceEntry<T>*>(resource.entry)->value = entry;

            // save this resource
            graph->resources.push_back(resource);

            // mark the resource to be created at the current pass
            auto& pass_node = graph->passes.at(pass);
            pass_node.creates.push_back(index);
            return index;
        }

        template <typename T>
        [[nodiscard]] FrameGraphResource create(const typename T::Descriptor& desc)
        {
            uint index           = static_cast<uint>(graph->resources.size());
            auto resource        = FrameGraphResourceNode{};
            resource.rsid        = index;
            resource.entry       = new FrameGraphResourceEntry<T>{};
            resource.entry->type = FrameGraphResourceType::TRANSIENT;

            // transient resources needs to remember the descriptor
            reinterpret_cast<FrameGraphResourceEntry<T>*>(resource.entry)->desc = desc;

            // save this resource
            graph->resources.push_back(resource);

            // mark the resource to be created at the current pass
            auto& pass_node = graph->passes.at(pass);
            pass_node.creates.push_back(index);
            return index;
        }

        // NOTE: Sometimes we need to read/write to the same resource in a single pass,
        // for example, updating a buffer in place. FrameGraph does not allow cycles,
        // therefore we will need to duplicate a resource logically, but under the hood
        // they are pointing to the same actual resource.
        [[nodiscard]] FrameGraphResource duplicate(FrameGraphResource rsid)
        {
            auto& from_resource = graph->resources.at(rsid);

            uint index         = static_cast<uint>(graph->resources.size());
            auto resource      = FrameGraphResourceNode{};
            resource.rsid      = index;
            resource.entry     = from_resource.entry;
            resource.duplicate = true; // explicitly mark the resource as a duplicated resource

            // save this resource
            graph->resources.push_back(resource);

            // mark the resource to be created at the current pass
            auto& pass_node = graph->passes.at(pass);
            pass_node.creates.push_back(index);
            return index;
        }

        [[nodiscard]] FrameGraphPass& create_pass(StringView name);

        [[nodiscard]] FrameGraphResource read(FrameGraphResource resource, FrameGraphReadOp op = FrameGraphReadOp::READ);
        [[nodiscard]] FrameGraphResource write(FrameGraphResource resource, FrameGraphWriteOp op = FrameGraphWriteOp::WRITE);
        [[nodiscard]] FrameGraphResource render(FrameGraphResource resource);
        [[nodiscard]] FrameGraphResource sample(FrameGraphResource resource);
        [[nodiscard]] FrameGraphResource present(FrameGraphResource resource);

        [[nodiscard]] auto build() -> Own<FrameGraph>;

    private:
        bool is_pass_valid() const { return pass != 0xFFFFFFFFu; }

    private:
        Own<FrameGraph> graph = nullptr;
        uint            pass  = 0xFFFFFFFFu;
    }; // end of FrameGraphBuilder

} // namespace lyra

#endif // LYRA_LIBRARY_RENDER_FRAME_GRAPH_BUILDER_H
