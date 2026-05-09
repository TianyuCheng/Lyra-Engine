#pragma once

#ifndef LYRA_LYRA_RENDER_FRAME_GRAPH_PASS_H
#define LYRA_LYRA_RENDER_FRAME_GRAPH_PASS_H

#include <Lyra/Render/FrameGraphCommon.h> // IWYU pragma: keep
#include <Lyra/Render/FrameGraphEnums.h>
#include <Lyra/Render/FrameGraphResource.h>

namespace lyra
{
    struct FrameGraphReadResource
    {
        FrameGraphResource resource;
        FrameGraphReadOp   read_op;
    };

    struct FrameGraphWriteResource
    {
        FrameGraphResource resource;
        FrameGraphWriteOp  write_op;
    };

    struct FrameGraphContext;
    struct FrameGraphPass
    {
    public:
        friend struct FrameGraph;
        friend struct FrameGraphBuilder;
        friend struct FrameGraphPassNode;

        FrameGraphPass(StringView name) : name(name) {}
        FrameGraphPass(FrameGraphPass&&)      = delete;
        FrameGraphPass(const FrameGraphPass&) = delete;

        template <typename T>
        using CompileCallback = typename std::function<T(FrameGraphPass&)>;

        template <typename T>
        auto compile(CompileCallback<T>&& f) -> T { return f(*this); }

        using ExecuteCallback = std::function<void(FrameGraphResources&, FrameGraphContext* ctx)>;
        void execute(ExecuteCallback&& f) { this->callback = std::move(f); }

        // prevent from being culled
        void preserve() { preserved = true; }

    private:
        String          name      = "";
        bool            preserved = false;
        ExecuteCallback callback;
    }; // end of FrameGraphPass

    struct FrameGraphPassNode
    {
        FrameGraphPass*                 entry   = {};
        uint                            psid    = 0;
        uint                            refcnt  = 0;
        Vector<FrameGraphReadResource>  reads   = {};
        Vector<FrameGraphWriteResource> writes  = {};
        Vector<FrameGraphResource>      creates = {};
        Vector<FrameGraphResource>      deletes = {};

        bool active() const { return refcnt != 0 || entry->preserved; }
    };

} // namespace lyra

#endif // LYRA_LYRA_RENDER_FRAME_GRAPH_PASS_H
