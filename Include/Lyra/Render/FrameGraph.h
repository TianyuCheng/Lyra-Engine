#pragma once

#ifndef LYRA_LYRA_RENDER_FRAME_GRAPH_H
#define LYRA_LYRA_RENDER_FRAME_GRAPH_H

#include <utility>
#include <type_traits>

#include <Lyra/Common/Hash.h>
#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/String.h>
#include <Lyra/Common/Pointer.h>
#include <Lyra/Common/Function.h>
#include <Lyra/Common/Collections.h>

#include <Lyra/Render/RHIHash.h>
#include <Lyra/Render/RHIDescs.h>
#include <Lyra/Render/RHIInits.h>
#include <Lyra/Render/RHITypes.h>

namespace lyra
{
    // -------------------------------------------------------------------------
    // enums
    // -------------------------------------------------------------------------

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

    // -------------------------------------------------------------------------
    // traits
    // -------------------------------------------------------------------------

    template <typename T, typename = void>
    struct has_pre_read : std::false_type
    {
    };

    template <typename T>
    struct has_pre_read<T, std::void_t<decltype(&T::pre_read)>> : std::true_type
    {
    };

    template <typename T>
    inline constexpr bool has_pre_read_v = has_pre_read<T>::value;

    template <typename T, typename = void>
    struct has_pre_write : std::false_type
    {
    };

    template <typename T>
    struct has_pre_write<T, std::void_t<decltype(&T::pre_write)>> : std::true_type
    {
    };

    template <typename T>
    inline constexpr bool has_pre_write_v = has_pre_write<T>::value;

    // -------------------------------------------------------------------------
    // resource
    // -------------------------------------------------------------------------

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
            if constexpr (has_pre_read_v<T>)
                value.pre_read(context, pass, op);
        }

        void pre_write(FrameGraphContext* context, FrameGraphPass* pass, FrameGraphWriteOp op) override
        {
            if constexpr (has_pre_write_v<T>)
                value.pre_write(context, pass, op);
        }
    };

    struct FrameGraphResourceNode
    {
        FrameGraphResourceModel* entry     = nullptr;
        uint                     rsid      = 0;
        uint                     refcnt    = 0;
        uint                     last_pass = ~0u;
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
            if (rsid >= data.size())
                data.resize(rsid + 1, nullptr);
            data[rsid] = resource;
        }

        template <typename T>
        const T* get(FrameGraphResource rsid) const
        {
            if (rsid >= data.size() || !data[rsid])
                return nullptr;

            auto resource = dynamic_cast<FrameGraphResourceEntry<T>*>(data[rsid]);
            assert(resource && "FrameGraphResources::get<T>: resource type mismatch!");
            return resource ? &resource->value : nullptr;
        }

    private:
        Vector<FrameGraphResourceModel*> data = {};
    };

    // -------------------------------------------------------------------------
    // pass
    // -------------------------------------------------------------------------

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

        using ExecuteCallback = Function<void(FrameGraphResources&, FrameGraphContext* ctx)>;
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

    // -------------------------------------------------------------------------
    // context
    // -------------------------------------------------------------------------

    struct FrameGraphContext
    {
        GPUDevice        device;
        GPUSurface       surface;
        GPUCommandBuffer cmdlist;
    };

    // -------------------------------------------------------------------------
    // allocator
    // -------------------------------------------------------------------------

    template <typename T>
    struct FrameGraphAllocatorEntry
    {
        T    data;
        bool used = false;
    };

    using FGBufferObject = GPUBufferHandle;
    using FGBufferVector = Vector<FrameGraphAllocatorEntry<FGBufferObject>>;

    using FGTextureObject = std::pair<GPUTextureHandle, GPUTextureViewHandle>;
    using FGTextureVector = Vector<FrameGraphAllocatorEntry<FGTextureObject>>;

    struct FrameGraphAllocator
    {
    public:
        auto allocate(const GPUBufferDescriptor& descriptor) -> FGBufferObject;
        void recycle(const GPUBufferDescriptor& descriptor, FGBufferObject buffer);

        auto allocate(const GPUTextureDescriptor& descriptor) -> FGTextureObject;
        void recycle(const GPUTextureDescriptor& descriptor, FGTextureObject texture);

    private:
        HashMap<GPUBufferDescriptor, FGBufferVector>   buffers;
        HashMap<GPUTextureDescriptor, FGTextureVector> textures;
    };

    // -------------------------------------------------------------------------
    // built-in resource types
    // -------------------------------------------------------------------------

    struct FrameGraphTexture
    {
        using Self       = FrameGraphTexture;
        using Descriptor = GPUTextureDescriptor;

        void create(FrameGraphAllocator* allocator, const Descriptor& descriptor)
        {
            auto handle = allocator->allocate(descriptor);
            texture     = handle.first;
            view        = handle.second;
            state       = undefined_state();
            format      = descriptor.format;
            layers      = descriptor.array_layers;
            levels      = descriptor.mip_level_count;
        }

        void destroy(FrameGraphAllocator* allocator, const Descriptor& descriptor)
        {
            auto handle = std::make_pair(texture, view);
            allocator->recycle(descriptor, handle);
            texture.reset();
            view.reset();
        }

        void pre_read(FrameGraphContext* context, FrameGraphPass* pass, FrameGraphReadOp op);
        void pre_write(FrameGraphContext* context, FrameGraphPass* pass, FrameGraphWriteOp op);

        // related texture handles
        GPUTextureHandle     texture;
        GPUTextureViewHandle view;
        GPUTextureFormat     format;
        uint                 layers = 1;
        uint                 levels = 1;
        TransitionState      state  = undefined_state();
    };

    struct FrameGraphBuffer
    {
        using Self       = FrameGraphBuffer;
        using Descriptor = GPUBufferDescriptor;

        void create(FrameGraphAllocator* allocator, const Descriptor& descriptor)
        {
            buffer = allocator->allocate(descriptor);
        }

        void destroy(FrameGraphAllocator* allocator, const Descriptor& descriptor)
        {
            allocator->recycle(descriptor, buffer);
            buffer.reset();
        }

        // related buffer handles
        GPUBufferHandle buffer;
    };

    // -------------------------------------------------------------------------
    // graph
    // -------------------------------------------------------------------------

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

    // -------------------------------------------------------------------------
    // builder
    // -------------------------------------------------------------------------

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

            // mark the resource to be registered in registry at the current pass
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

            // mark the resource to be registered at the current pass
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

#endif // LYRA_LYRA_RENDER_FRAME_GRAPH_H
