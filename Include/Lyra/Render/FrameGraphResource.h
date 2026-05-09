#pragma once

#ifndef LYRA_LYRA_RENDER_FRAME_GRAPH_RESOURCE_H
#define LYRA_LYRA_RENDER_FRAME_GRAPH_RESOURCE_H

#include <Lyra/Render/FrameGraphCommon.h> // IWYU pragma: keep
#include <Lyra/Render/FrameGraphEnums.h>
#include <Lyra/Render/FrameGraphTraits.h>

namespace lyra
{
    using FrameGraphResource = std::uint32_t;

    struct FrameGraphPass;
    struct FrameGraphContext;
    struct FrameGraphAllocator;

    struct FrameGraphResourceModel
    {
        FrameGraphResourceType type = FrameGraphResourceType::TRANSIENT;

        virtual ~FrameGraphResourceModel()                                                             = default;
        virtual void create(FrameGraphAllocator* allocator)                                            = 0;
        virtual void destroy(FrameGraphAllocator* allocator)                                           = 0;
        virtual void pre_read(FrameGraphContext* context, FrameGraphPass* pass, FrameGraphReadOp op)   = 0;
        virtual void pre_write(FrameGraphContext* context, FrameGraphPass* pass, FrameGraphWriteOp op) = 0;
    };

    template <typename T>
    struct FrameGraphResourceEntry : public FrameGraphResourceModel
    {
        typename T::Descriptor desc;
        T                      value;

        void create(FrameGraphAllocator* allocator) override
        {
            if (type == FrameGraphResourceType::TRANSIENT)
                value.create(allocator, desc);
        }

        void destroy(FrameGraphAllocator* allocator) override
        {
            if (type == FrameGraphResourceType::TRANSIENT)
                value.destroy(allocator, desc);
        }

        void pre_read(FrameGraphContext* context, FrameGraphPass* pass, FrameGraphReadOp op) override
        {
            if constexpr (has_pre_read<T>::value)
                value.pre_read(context, pass, op);
        }

        void pre_write(FrameGraphContext* context, FrameGraphPass* pass, FrameGraphWriteOp op) override
        {
            if constexpr (has_pre_write<T>::value)
                value.pre_write(context, pass, op);
        }
    };

    struct FrameGraphResourceNode
    {
        FrameGraphResourceModel* entry     = nullptr;
        uint                     rsid      = 0;
        uint                     refcnt    = 0;
        uint                     last_pass = 0;
        bool                     duplicate = false;
        Vector<uint>             consumers = {};
        Vector<uint>             producers = {};
    };

    struct FrameGraphResources
    {
    public:
        friend struct FrameGraph;

        void put(FrameGraphResource rsid, FrameGraphResourceModel* resource)
        {
            data[rsid] = resource;
        }

        template <typename T>
        const T* get(FrameGraphResource rsid) const
        {
            auto it = data.find(rsid);
            if (it == data.end())
                return nullptr;

            auto resource = dynamic_cast<FrameGraphResourceEntry<T>*>(it->second);
            return &resource->value;
        }

    private:
        HashMap<FrameGraphResource, FrameGraphResourceModel*> data = {};
    };

} // namespace lyra

#endif // LYRA_LYRA_RENDER_FRAME_GRAPH_RESOURCE_H
