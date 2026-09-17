#pragma once

#ifndef LYRA_LYRA_RENDER_FRAME_GRAPH_H
#define LYRA_LYRA_RENDER_FRAME_GRAPH_H

#include <utility>
#include <type_traits>
#include <cassert>

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
    // type identification & traits
    // -------------------------------------------------------------------------

    using ResourceTypeID = const void*;

    template <typename T>
    inline ResourceTypeID get_resource_type_id()
    {
        static const char id = 0;
        return &id;
    }

    struct FrameGraphPass;
    struct FrameGraphContext;
    struct FrameGraphAllocator;
    struct FrameGraphBarrierBatch;

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

    template <typename T, typename = void>
    struct has_batched_pre_read : std::false_type
    {
    };

    template <typename T>
    struct has_batched_pre_read<T, std::void_t<decltype(std::declval<T>().pre_read(
        std::declval<FrameGraphContext*>(),
        std::declval<FrameGraphPass*>(),
        std::declval<FrameGraphReadOp>(),
        std::declval<FrameGraphBarrierBatch*>()
    ))>> : std::true_type
    {
    };

    template <typename T>
    inline constexpr bool has_batched_pre_read_v = has_batched_pre_read<T>::value;

    template <typename T, typename = void>
    struct has_batched_pre_write : std::false_type
    {
    };

    template <typename T>
    struct has_batched_pre_write<T, std::void_t<decltype(std::declval<T>().pre_write(
        std::declval<FrameGraphContext*>(),
        std::declval<FrameGraphPass*>(),
        std::declval<FrameGraphWriteOp>(),
        std::declval<FrameGraphBarrierBatch*>()
    ))>> : std::true_type
    {
    };

    template <typename T>
    inline constexpr bool has_batched_pre_write_v = has_batched_pre_write<T>::value;

    // -------------------------------------------------------------------------
    // strongly-typed resource handle (supports SSA versioning)
    // -------------------------------------------------------------------------

    template <typename T = void>
    struct FrameGraphHandle
    {
        uint id      = ~0u;
        uint version = 0;

        constexpr FrameGraphHandle() = default;
        constexpr explicit FrameGraphHandle(uint id, uint version = 0) : id(id), version(version) {}

        // allow implicit conversion to untyped handle, or from same-type
        template <typename U, typename = std::enable_if_t<std::is_void_v<T> || std::is_same_v<T, U>>>
        constexpr FrameGraphHandle(const FrameGraphHandle<U>& other)
            : id(other.id), version(other.version) {}

        template <typename U>
        [[nodiscard]] constexpr FrameGraphHandle<U> cast() const noexcept
        {
            return FrameGraphHandle<U>{id, version};
        }

        [[nodiscard]] constexpr uint rsid() const noexcept { return id; }
        [[nodiscard]] constexpr bool valid() const noexcept { return id != ~0u; }
        constexpr void reset() noexcept { id = ~0u; version = 0; }

        constexpr bool operator==(const FrameGraphHandle& other) const noexcept
        {
            return id == other.id && version == other.version;
        }

        constexpr bool operator!=(const FrameGraphHandle& other) const noexcept
        {
            return !(*this == other);
        }
    };

    using FrameGraphResource = FrameGraphHandle<void>;

    // -------------------------------------------------------------------------
    // barrier batching
    // -------------------------------------------------------------------------

    struct FrameGraphBarrierBatch
    {
        Vector<GPUTextureBarrier> texture_barriers;
        Vector<GPUBufferBarrier>  buffer_barriers;

        void add_texture_barrier(GPUTextureBarrier barrier)
        {
            texture_barriers.push_back(barrier);
        }

        void add_buffer_barrier(GPUBufferBarrier barrier)
        {
            buffer_barriers.push_back(barrier);
        }

        void submit(const GPUCommandBuffer& cmdlist)
        {
            if (!texture_barriers.empty()) {
                cmdlist.resource_barrier(texture_barriers);
                texture_barriers.clear();
            }
            if (!buffer_barriers.empty()) {
                cmdlist.resource_barrier(buffer_barriers);
                buffer_barriers.clear();
            }
        }
    };

    // -------------------------------------------------------------------------
    // resource model & entries
    // -------------------------------------------------------------------------

    struct FrameGraphResourceModel
    {
        FrameGraphResourceType type = FrameGraphResourceType::TRANSIENT;

        virtual ~FrameGraphResourceModel()                                                                                     = default;
        virtual ResourceTypeID get_type_id() const                                                                             = 0;
        virtual void create(FrameGraphAllocator* allocator)                                                                    = 0;
        virtual void destroy(FrameGraphAllocator* allocator)                                                                   = 0;
        virtual void pre_read(FrameGraphContext* context, FrameGraphPass* pass, FrameGraphReadOp op, FrameGraphBarrierBatch* barriers)   = 0;
        virtual void pre_write(FrameGraphContext* context, FrameGraphPass* pass, FrameGraphWriteOp op, FrameGraphBarrierBatch* barriers) = 0;
    };

    template <typename T>
    struct FrameGraphResourceEntry : public FrameGraphResourceModel
    {
        typename T::Descriptor desc;
        T                      value;

        ResourceTypeID get_type_id() const override
        {
            return get_resource_type_id<T>();
        }

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

        void pre_read(FrameGraphContext* context, FrameGraphPass* pass, FrameGraphReadOp op, FrameGraphBarrierBatch* barriers) override
        {
            if constexpr (has_batched_pre_read_v<T>)
                value.pre_read(context, pass, op, barriers);
            else if constexpr (has_pre_read_v<T>)
                value.pre_read(context, pass, op);
        }

        void pre_write(FrameGraphContext* context, FrameGraphPass* pass, FrameGraphWriteOp op, FrameGraphBarrierBatch* barriers) override
        {
            if constexpr (has_batched_pre_write_v<T>)
                value.pre_write(context, pass, op, barriers);
            else if constexpr (has_pre_write_v<T>)
                value.pre_write(context, pass, op);
        }
    };

    struct FrameGraphResourceNode
    {
        FrameGraphResourceModel* entry        = nullptr;
        uint                     rsid         = 0;
        uint                     physical_id  = 0;
        uint                     creator_pass = 0;
        uint                     refcnt       = 0;
        uint                     last_pass    = ~0u;
        bool                     duplicate    = false;
        Vector<uint>             consumers    = {};
        Vector<uint>             producers    = {};
    };

    struct FrameGraphResources
    {
    public:
        friend struct FrameGraph;

        void put(uint rsid, FrameGraphResourceModel* resource)
        {
            if (rsid >= data.size())
                data.resize(rsid + 1, nullptr);
            data[rsid] = resource;
        }

        template <typename T>
        const T* get(FrameGraphHandle<T> handle) const
        {
            return get<T>(handle.id);
        }

        template <typename T>
        T* get_mut(FrameGraphHandle<T> handle)
        {
            return get_mut<T>(handle.id);
        }

        template <typename T>
        const T* get(FrameGraphResource handle) const
        {
            return get<T>(handle.id);
        }

        template <typename T>
        T* get_mut(FrameGraphResource handle)
        {
            return get_mut<T>(handle.id);
        }

        template <typename T>
        const T* get(uint rsid) const
        {
            if (rsid >= data.size() || !data[rsid])
                return nullptr;

            assert(data[rsid]->get_type_id() == get_resource_type_id<T>() && "FrameGraphResources::get<T>: resource type mismatch!");
            auto* resource = static_cast<FrameGraphResourceEntry<T>*>(data[rsid]);
            return resource ? &resource->value : nullptr;
        }

        template <typename T>
        T* get_mut(uint rsid)
        {
            if (rsid >= data.size() || !data[rsid])
                return nullptr;

            assert(data[rsid]->get_type_id() == get_resource_type_id<T>() && "FrameGraphResources::get_mut<T>: resource type mismatch!");
            auto* resource = static_cast<FrameGraphResourceEntry<T>*>(data[rsid]);
            return resource ? &resource->value : nullptr;
        }

        void reset(size_t size)
        {
            data.assign(size, nullptr);
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
        friend struct FrameGraphPassBuilder;

        FrameGraphPass(StringView name) : name(name) {}
        FrameGraphPass(FrameGraphPass&&)                 = delete;
        FrameGraphPass(const FrameGraphPass&)            = delete;
        FrameGraphPass& operator=(const FrameGraphPass&) = delete;
        FrameGraphPass& operator=(FrameGraphPass&&)      = delete;

        template <typename T = void, typename F>
        auto compile(F&& f) -> T
        {
            if constexpr (std::is_void_v<T>) {
                f(*this);
            } else {
                return f(*this);
            }
        }

        using ExecuteCallback = Function<void(FrameGraphResources&, FrameGraphContext* ctx)>;
        void execute(ExecuteCallback&& f) { this->callback = std::move(f); }

        // prevent from being culled
        void preserve() { preserved = true; }
        [[nodiscard]] bool is_preserved() const { return preserved; }
        [[nodiscard]] StringView get_name() const { return name; }

    private:
        String          name      = "";
        bool            preserved = false;
        ExecuteCallback callback;
    };

    struct FrameGraphPassNode
    {
        Own<FrameGraphPass>             entry   = nullptr;
        uint                            psid    = 0;
        uint                            refcnt  = 0;
        Vector<FrameGraphReadResource>  reads   = {};
        Vector<FrameGraphWriteResource> writes  = {};
        Vector<FrameGraphResource>      creates = {};
        Vector<FrameGraphResource>      deletes = {};

        bool active() const { return refcnt != 0 || (entry && entry->is_preserved()); }
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

    struct FGTextureObject
    {
        GPUTextureHandle     texture;
        GPUTextureViewHandle view;

        bool operator==(const FGTextureObject& other) const
        {
            return texture == other.texture && view == other.view;
        }

        bool operator!=(const FGTextureObject& other) const
        {
            return !(*this == other);
        }
    };

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
            texture     = handle.texture;
            view        = handle.view;
            state       = undefined_state();
            format      = descriptor.format;
            layers      = descriptor.array_layers;
            levels      = descriptor.mip_level_count;
        }

        void destroy(FrameGraphAllocator* allocator, const Descriptor& descriptor)
        {
            auto handle = FGTextureObject{texture, view};
            allocator->recycle(descriptor, handle);
            texture.reset();
            view.reset();
        }

        void pre_read(FrameGraphContext* context, FrameGraphPass* pass, FrameGraphReadOp op, FrameGraphBarrierBatch* barriers = nullptr);
        void pre_write(FrameGraphContext* context, FrameGraphPass* pass, FrameGraphWriteOp op, FrameGraphBarrierBatch* barriers = nullptr);

        GPUTextureHandle     texture;
        GPUTextureViewHandle view;
        GPUTextureFormat     format{};
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

        void pre_read(FrameGraphContext* context, FrameGraphPass* pass, FrameGraphReadOp op, FrameGraphBarrierBatch* barriers = nullptr);
        void pre_write(FrameGraphContext* context, FrameGraphPass* pass, FrameGraphWriteOp op, FrameGraphBarrierBatch* barriers = nullptr);

        GPUBufferHandle buffer;
    };

    // -------------------------------------------------------------------------
    // graph
    // -------------------------------------------------------------------------

    struct FrameGraphBuilder;
    struct FrameGraphPassBuilder;

    struct FrameGraph
    {
    public:
        friend struct FrameGraphBuilder;
        friend struct FrameGraphPassBuilder;

        using Pass          = FrameGraphPass;
        using Builder       = FrameGraphBuilder;
        using PassBuilder   = FrameGraphPassBuilder;
        using Context       = FrameGraphContext;
        using Resource      = FrameGraphResource;
        using Resources     = FrameGraphResources;
        using Buffer        = FrameGraphBuffer;
        using Texture       = FrameGraphTexture;
        using Allocator     = FrameGraphAllocator;
        template <typename T>
        using Handle        = FrameGraphHandle<T>;
        using TextureHandle = FrameGraphHandle<FrameGraphTexture>;
        using BufferHandle  = FrameGraphHandle<FrameGraphBuffer>;

        explicit FrameGraph()                   = default;
        explicit FrameGraph(FrameGraph&&)       = delete;
        explicit FrameGraph(const FrameGraph&)  = delete;
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
        Vector<FrameGraphPassNode>           passes;
        Vector<FrameGraphResourceNode>       resources;
        Vector<Own<FrameGraphResourceModel>> physical_resources;
        Vector<uint>                         execution_order;
        FrameGraphResources                  registry;
    };

    // -------------------------------------------------------------------------
    // pass builder
    // -------------------------------------------------------------------------

    struct FrameGraphPassBuilder
    {
    public:
        FrameGraphPassBuilder(FrameGraph& graph, uint pass_id)
            : graph(graph), pass_id(pass_id) {}

        template <typename T>
        [[nodiscard]] FrameGraphHandle<T> import(const T& entry)
        {
            uint index            = static_cast<uint>(graph.resources.size());
            auto physical         = std::make_unique<FrameGraphResourceEntry<T>>();
            physical->type        = FrameGraphResourceType::IMPORTED;
            physical->value       = entry;

            uint physical_id      = static_cast<uint>(graph.physical_resources.size());
            graph.physical_resources.push_back(std::move(physical));

            auto resource         = FrameGraphResourceNode{};
            resource.rsid         = index;
            resource.physical_id  = physical_id;
            resource.creator_pass = pass_id;
            resource.entry        = graph.physical_resources.back().get();

            graph.resources.push_back(resource);

            auto& pass_node = graph.passes.at(pass_id);
            pass_node.creates.push_back(FrameGraphResource{index});
            return FrameGraphHandle<T>{index};
        }

        template <typename T>
        [[nodiscard]] FrameGraphHandle<T> create(const typename T::Descriptor& desc)
        {
            uint index            = static_cast<uint>(graph.resources.size());
            auto physical         = std::make_unique<FrameGraphResourceEntry<T>>();
            physical->type        = FrameGraphResourceType::TRANSIENT;
            physical->desc        = desc;

            uint physical_id      = static_cast<uint>(graph.physical_resources.size());
            graph.physical_resources.push_back(std::move(physical));

            auto resource         = FrameGraphResourceNode{};
            resource.rsid         = index;
            resource.physical_id  = physical_id;
            resource.creator_pass = pass_id;
            resource.entry        = graph.physical_resources.back().get();

            graph.resources.push_back(resource);

            auto& pass_node = graph.passes.at(pass_id);
            pass_node.creates.push_back(FrameGraphResource{index});
            return FrameGraphHandle<T>{index};
        }

        template <typename T = void>
        [[nodiscard]] FrameGraphHandle<T> duplicate(FrameGraphHandle<T> resource)
        {
            auto& from_resource   = graph.resources.at(resource.id);

            uint index            = static_cast<uint>(graph.resources.size());
            auto res_node         = FrameGraphResourceNode{};
            res_node.rsid         = index;
            res_node.physical_id  = from_resource.physical_id;
            res_node.creator_pass = pass_id;
            res_node.entry        = from_resource.entry;
            res_node.duplicate    = true;

            graph.resources.push_back(res_node);

            auto& pass_node = graph.passes.at(pass_id);
            pass_node.creates.push_back(FrameGraphResource{index});
            return FrameGraphHandle<T>{index, resource.version + 1};
        }

        template <typename T = void>
        [[nodiscard]] FrameGraphHandle<T> read(FrameGraphHandle<T> resource, FrameGraphReadOp op = FrameGraphReadOp::READ)
        {
            auto  read_resource = FrameGraphReadResource{FrameGraphResource{resource.id, resource.version}, op};
            auto& pass_node     = graph.passes.at(pass_id);
            pass_node.reads.push_back(read_resource);

            auto& resource_node = graph.resources.at(resource.id);
            resource_node.consumers.push_back(pass_id);
            resource_node.last_pass = pass_id;

            return resource;
        }

        template <typename T = void>
        [[nodiscard]] FrameGraphHandle<T> write(FrameGraphHandle<T> resource, FrameGraphWriteOp op = FrameGraphWriteOp::WRITE)
        {
            auto  write_resource = FrameGraphWriteResource{FrameGraphResource{resource.id, resource.version}, op};
            auto& pass_node      = graph.passes.at(pass_id);
            pass_node.writes.push_back(write_resource);

            auto& resource_node = graph.resources.at(resource.id);
            resource_node.producers.push_back(pass_id);
            resource_node.last_pass = pass_id;

            return FrameGraphHandle<T>{resource.id, resource.version + 1};
        }

        template <typename T = void>
        [[nodiscard]] FrameGraphHandle<T> render(FrameGraphHandle<T> resource)
        {
            return write(resource, FrameGraphWriteOp::RENDER);
        }

        template <typename T = void>
        [[nodiscard]] FrameGraphHandle<T> sample(FrameGraphHandle<T> resource)
        {
            return read(resource, FrameGraphReadOp::SAMPLE);
        }

        template <typename T = void>
        [[nodiscard]] FrameGraphHandle<T> present(FrameGraphHandle<T> resource)
        {
            return read(resource, FrameGraphReadOp::PRESENT);
        }

        void preserve()
        {
            auto& pass_node = graph.passes.at(pass_id);
            if (pass_node.entry)
                pass_node.entry->preserve();
        }

    private:
        FrameGraph& graph;
        uint        pass_id;
    };

    // -------------------------------------------------------------------------
    // builder
    // -------------------------------------------------------------------------

    struct FrameGraphBuilder
    {
    public:
        explicit FrameGraphBuilder();
        FrameGraphBuilder(FrameGraphBuilder&&)                 = delete;
        FrameGraphBuilder(const FrameGraphBuilder&)            = delete;
        FrameGraphBuilder&  operator=(const FrameGraphBuilder&) = delete;
        FrameGraphBuilder&& operator=(FrameGraphBuilder&&)     = delete;
        virtual ~FrameGraphBuilder()                           = default;

        // modern idiomatic pass declaration with PassData
        template <typename Data, typename Setup, typename Exec>
        Data add_pass(StringView name, Setup&& setup, Exec&& exec)
        {
            auto& pass_entry = create_pass(name);
            Data data{};
            FrameGraphPassBuilder pass_builder(*graph, this->pass);
            setup(data, pass_builder);

            pass_entry.execute([data, exec = std::forward<Exec>(exec)](FrameGraphResources& resources, FrameGraphContext* ctx) {
                exec(data, resources, ctx);
            });
            return data;
        }

        // modern idiomatic pass declaration without PassData
        template <typename Setup, typename Exec>
        void add_pass(StringView name, Setup&& setup, Exec&& exec)
        {
            auto& pass_entry = create_pass(name);
            FrameGraphPassBuilder pass_builder(*graph, this->pass);
            setup(pass_builder);

            pass_entry.execute([exec = std::forward<Exec>(exec)](FrameGraphResources& resources, FrameGraphContext* ctx) {
                exec(resources, ctx);
            });
        }

        [[nodiscard]] FrameGraphPass& create_pass(StringView name);

        template <typename T>
        [[nodiscard]] FrameGraphHandle<T> import(const T& entry)
        {
            assert(is_pass_valid() && "must call FrameGraphBuilder::create_pass(...) prior to import(...)");
            FrameGraphPassBuilder pb(*graph, pass);
            return pb.import(entry);
        }

        template <typename T>
        [[nodiscard]] FrameGraphHandle<T> create(const typename T::Descriptor& desc)
        {
            assert(is_pass_valid() && "must call FrameGraphBuilder::create_pass(...) prior to create(...)");
            FrameGraphPassBuilder pb(*graph, pass);
            return pb.create<T>(desc);
        }

        template <typename T = void>
        [[nodiscard]] FrameGraphHandle<T> duplicate(FrameGraphHandle<T> resource)
        {
            assert(is_pass_valid() && "must call FrameGraphBuilder::create_pass(...) prior to duplicate(...)");
            FrameGraphPassBuilder pb(*graph, pass);
            return pb.duplicate(resource);
        }

        template <typename T = void>
        [[nodiscard]] FrameGraphHandle<T> read(FrameGraphHandle<T> resource, FrameGraphReadOp op = FrameGraphReadOp::READ)
        {
            assert(is_pass_valid() && "must call FrameGraphBuilder::create_pass(...) prior to read(...)");
            FrameGraphPassBuilder pb(*graph, pass);
            return pb.read(resource, op);
        }

        template <typename T = void>
        [[nodiscard]] FrameGraphHandle<T> write(FrameGraphHandle<T> resource, FrameGraphWriteOp op = FrameGraphWriteOp::WRITE)
        {
            assert(is_pass_valid() && "must call FrameGraphBuilder::create_pass(...) prior to write(...)");
            FrameGraphPassBuilder pb(*graph, pass);
            return pb.write(resource, op);
        }

        template <typename T = void>
        [[nodiscard]] FrameGraphHandle<T> render(FrameGraphHandle<T> resource)
        {
            assert(is_pass_valid() && "must call FrameGraphBuilder::create_pass(...) prior to render(...)");
            FrameGraphPassBuilder pb(*graph, pass);
            return pb.render(resource);
        }

        template <typename T = void>
        [[nodiscard]] FrameGraphHandle<T> sample(FrameGraphHandle<T> resource)
        {
            assert(is_pass_valid() && "must call FrameGraphBuilder::create_pass(...) prior to sample(...)");
            FrameGraphPassBuilder pb(*graph, pass);
            return pb.sample(resource);
        }

        template <typename T = void>
        [[nodiscard]] FrameGraphHandle<T> present(FrameGraphHandle<T> resource)
        {
            assert(is_pass_valid() && "must call FrameGraphBuilder::create_pass(...) prior to present(...)");
            FrameGraphPassBuilder pb(*graph, pass);
            return pb.present(resource);
        }

        [[nodiscard]] auto build() -> Own<FrameGraph>;

    private:
        bool is_pass_valid() const { return pass != 0xFFFFFFFFu; }

    private:
        Own<FrameGraph> graph = nullptr;
        uint            pass  = 0xFFFFFFFFu;
    };

} // namespace lyra

#endif // LYRA_LYRA_RENDER_FRAME_GRAPH_H
