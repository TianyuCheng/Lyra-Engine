#ifndef LYRA_PLUGIN_D3D12_UTILS_H
#define LYRA_PLUGIN_D3D12_UTILS_H

#include <D3D12MemAlloc.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <limits>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

#include <Lyra/Common/Enums.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Msgbox.h>
#include <Lyra/Common/Memory.h>
#include <Lyra/Common/Conversion.h>
#include <Lyra/Common/Collections.h>
#include <Lyra/Common/Compatibility.h>
#include <Lyra/Render/RHIDescs.h>
#include <Lyra/Render/RHIAPI.h>
#include <Lyra/Render/RHIError.h>

#include "SimpleHeap.h"
#include "BlockAllocator.h"

using namespace lyra;

// No direct mapping from GPUPresentMode,
// but it is a combination of swap effect and sync interval.
struct D3D12PresentMode
{
    uint sync_interval = 1;
    uint sync_flags    = 0;
};

template <typename T>
struct D3D12Destroyer
{
    void operator()(T& obj)
    {
        obj.destroy();
    }
};

template <typename T>
using D3D12ResourceManager = Slotmap<T, D3D12Destroyer<T>>;

struct D3D12CPUDescriptor
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle{0};
    uint                        pool  = 0; // pool index
    uint                        index = 0; // index within pool

    D3D12CPUDescriptor() { reset(); }

    bool valid() const { return handle.ptr != 0; }

    void reset()
    {
        handle.ptr = 0;
        pool       = 0;
        index      = 0;
    }
};

// This is a helper class for non-growable d3d12 descriptor pool on CPU side.
struct D3D12ObjectPool
{
    ID3D12DescriptorHeap* heap      = nullptr;
    uint                  increment = 0;
    uint                  capacity  = 0;
    uint                  count     = 0;
    Vector<uint>          freed     = {};

    void init(uint capacity, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags);
    void reset();
    void destroy();
    auto allocate() -> D3D12CPUDescriptor;
    void recycle(uint index);
};

// This is a helper class for growable d3d12 descriptor pool on CPU side.
struct D3D12ObjectHeap
{
    Vector<D3D12ObjectPool>     heaps      = {};
    uint                        heap_index = 0;
    uint                        capacity   = 0;
    D3D12_DESCRIPTOR_HEAP_TYPE  heap_type  = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
    D3D12_DESCRIPTOR_HEAP_FLAGS heap_flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    void init(uint capacity, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags);
    void reset();
    void destroy();
    auto allocate() -> D3D12CPUDescriptor;
    void recycle(const D3D12CPUDescriptor& descriptor);
    uint find_pool_index();
};

// This is a helper class for non-growable d3d12 descriptor pool on GPU side.
struct D3D12BindGroupHeapAllocator;
struct D3D12DescriptorPool
{
    D3D12BindGroupHeapAllocator* allocator = nullptr;
    AllocationHandle             allocation;
    uint                         capacity = 0u;
    uint                         count    = 0u;

    void init(D3D12BindGroupHeapAllocator* allocator, uint capacity);
    uint allocate(uint allocate_count = 1);
    void reset();
    void destroy();
};

// This is a helper class for growable d3d12 descriptor pool on GPU side.
struct D3D12BindGroupHeapAllocator;
struct D3D12DescriptorHeap
{
    D3D12BindGroupHeapAllocator* allocator = nullptr;
    Vector<D3D12DescriptorPool>  pools;
    uint                         page_size = 0ull;

    void init(D3D12BindGroupHeapAllocator* allocator, uint page_size);
    void reset();
    void destroy();
    uint allocate(uint allocate_count = 1);

    auto create_new_pool(uint allocate_count) -> D3D12DescriptorPool&;
    auto find_available_pool(uint allocate_count) -> D3D12DescriptorPool&;

    // complaint with D3D12ResourceManager
    bool valid() const { return true; }
};

// This is a helper to allocate a giant descriptor heap, and then sub-allocate.
struct D3D12BindGroupHeapAllocator
{
    ID3D12DescriptorHeap*     heap = nullptr;
    BlockAllocator<std::byte> allocator;
    uint                      capacity  = 0;
    uint                      increment = 0;

    void init(uint capacity, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags);
    void reset();
    void destroy();

    void resize(); // resize (and copy the existing descriptors over)

    auto cpu(uint index) const -> D3D12_CPU_DESCRIPTOR_HANDLE;
    auto gpu(uint index) const -> D3D12_GPU_DESCRIPTOR_HANDLE;
};

// D3D12's Fence is similar to d3d12's Timeline Semaphore,
// it can be used for both CPU/GPU, and GPU/GPU synchronization.
struct D3D12Fence
{
    ID3D12Fence* fence = nullptr;
    HANDLE       event;

    mutable uint64_t target = 0ull;

    // implementation in D3D12Fence.cpp
    explicit D3D12Fence();
    explicit D3D12Fence(bool signaled);

    void init(bool signaled);
    void wait(uint64_t timeout = UINT64_MAX);
    void reset();
    bool ready();
    void signal(ID3D12CommandQueue* queue, uint64_t value);
    void destroy();

    bool valid() const { return fence != nullptr; }
};

struct D3D12Buffer
{
    ID3D12Resource*      buffer     = nullptr;
    D3D12MA::Allocation* allocation = nullptr;

    uint64_t size_       = 0ull;
    uint8_t* mapped_data = nullptr;
    uint64_t mapped_size = 0ull;

    // implementation in D3D12Buffer.cpp
    explicit D3D12Buffer();
    explicit D3D12Buffer(const GPUBufferDescriptor& desc, D3D12_RESOURCE_FLAGS additional_flags = D3D12_RESOURCE_FLAG_NONE);

    void map(GPUSize64 offset = 0, GPUSize64 size = 0);
    void unmap();
    bool mapped() const { return mapped_data != nullptr; }
    void destroy();
    auto size() const -> size_t { return size_; }

    bool valid() const { return buffer != nullptr; }

    template <typename T>
    auto get_mapped_data_typed() -> T*
    {
        return reinterpret_cast<T*>(mapped_data);
    }

    template <typename T>
    auto map(GPUSize64 offset = 0, GPUSize64 size = 0)
    {
        map(offset, size);
        return get_mapped_data_typed<T>();
    }
};

struct D3D12TextureView;
struct D3D12Texture
{
    ID3D12Resource*      texture    = nullptr;
    D3D12MA::Allocation* allocation = nullptr;
    DXGI_FORMAT          format     = DXGI_FORMAT_UNKNOWN;
    GPUExtent2D          area       = {};
    GPUTextureUsageFlags usages     = 0;
    uint                 samples    = 1;

    // implementation in D3D12Texture.cpp
    explicit D3D12Texture();
    explicit D3D12Texture(ID3D12Resource* texture);
    explicit D3D12Texture(const GPUTextureDescriptor& desc);

    void destroy();

    bool valid() const { return texture != nullptr; }
};

struct D3D12TextureView
{
    // unlike d3d12's VkImageView, D3D12 does not have native concept for it.
    // It treats image view as descriptors directly. VkImageView does not
    // differentiate between different usages, so the same image view can
    // be bound for different usages. For D3D12 we will have to create multiple
    // descriptors if there are multiple usages.
    union
    {
        // rtv and dsv share the same view
        D3D12CPUDescriptor rtv_view = {};
        D3D12CPUDescriptor dsv_view;
    };
    D3D12CPUDescriptor srv_view = {};
    D3D12CPUDescriptor uav_view = {};

    GPUExtent2D area; // used only for RenderArea

    // implementation in D3D12Texture.cpp
    explicit D3D12TextureView();
    explicit D3D12TextureView(const D3D12Texture& texture, const GPUTextureViewDescriptor& desc);

    void init_rtv(const D3D12Texture& texture, const GPUTextureViewDescriptor& desc);
    void init_dsv(const D3D12Texture& texture, const GPUTextureViewDescriptor& desc);
    void init_srv(const D3D12Texture& texture, const GPUTextureViewDescriptor& desc);
    void init_uav(const D3D12Texture& texture, const GPUTextureViewDescriptor& desc);

    void destroy();

    bool valid() const
    {
        return rtv_view.valid() || srv_view.valid() || uav_view.valid();
    }
};

struct D3D12Sampler
{
    D3D12CPUDescriptor sampler;

    // implementation in D3D12Sampler.cpp
    explicit D3D12Sampler();
    explicit D3D12Sampler(const GPUSamplerDescriptor& desc);

    void destroy();

    bool valid() const { return sampler.valid(); }
};

struct D3D12Shader
{
    std::vector<uint8_t> binary;

    // implementation in D3D12Shader.cpp
    explicit D3D12Shader();
    explicit D3D12Shader(const GPUShaderModuleDescriptor& desc);

    void destroy();

    bool valid() const { return !binary.empty(); }
};

struct D3D12BindGroup
{
    // NOTE: D3D12 requires an explicit separation of cbv_srv_uav vs sampler heap,
    // but a single bindgroup is allowed to contain both. We only need to record
    // the index into the heap though
    uint32_t default_index = std::numeric_limits<uint32_t>::max();
    uint32_t sampler_index = std::numeric_limits<uint16_t>::max();
    uint16_t dynamic_index = std::numeric_limits<uint16_t>::max();
    uint16_t heap_index    = std::numeric_limits<uint16_t>::max();

    bool valid() const
    {
        bool default_valid = default_index != std::numeric_limits<uint32_t>::max();
        bool sampler_valid = sampler_index != std::numeric_limits<uint16_t>::max();
        bool dynamic_valid = dynamic_index != std::numeric_limits<uint16_t>::max();
        bool heap_valid    = heap_index != std::numeric_limits<uint16_t>::max();
        return default_valid || sampler_valid || (dynamic_valid && heap_valid);
    }
};

struct D3D12BindGroupDynamic
{
    D3D12_ROOT_PARAMETER_TYPE type;
    D3D12_GPU_VIRTUAL_ADDRESS address;
};

struct D3D12BindInfo
{
    D3D12_DESCRIPTOR_RANGE_TYPE type    = (D3D12_DESCRIPTOR_RANGE_TYPE)0;
    uint                        start   = 0;
    uint                        count   = 0;
    bool                        dynamic = false;
};

struct D3D12BindGroupInfo
{
    uint32_t default_root_parameter = std::numeric_limits<uint32_t>::max();
    uint16_t sampler_root_parameter = std::numeric_limits<uint16_t>::max();
    uint16_t dynamic_root_parameter = std::numeric_limits<uint16_t>::max();

    bool has_default_root_parameter() const { return default_root_parameter != std::numeric_limits<uint32_t>::max(); }
    bool has_sampler_root_parameter() const { return sampler_root_parameter != std::numeric_limits<uint16_t>::max(); }
    bool has_dynamic_root_parameter() const { return dynamic_root_parameter != std::numeric_limits<uint16_t>::max(); }
};

struct D3D12BindGroupHeap
{
    D3D12DescriptorHeap         default_heap;
    D3D12DescriptorHeap         sampler_heap;
    Heap<D3D12BindGroupDynamic> dynamic_heap;

    // allocated memory descriptors will all come from here
    Ref<MemoryArena> memory;

    explicit D3D12BindGroupHeap();
    explicit D3D12BindGroupHeap(const GPUBindGroupHeapDescriptor& desc);
    virtual ~D3D12BindGroupHeap();

    // complaint with D3D12ResourceManager
    bool valid() const { return true; }

    void reset()
    {
        default_heap.reset();
        sampler_heap.reset();
        dynamic_heap.reset();
        if (memory)
            memory->reset();
    }

    void destroy()
    {
        reset();
        default_heap.destroy();
        sampler_heap.destroy();
        if (memory)
            memory->destroy();
    }
};

struct D3D12BindGroupLayout
{
    Vector<D3D12_DESCRIPTOR_RANGE1> sampler_ranges = {};
    Vector<D3D12_DESCRIPTOR_RANGE1> default_ranges = {};
    Vector<D3D12_DESCRIPTOR_RANGE1> dynamic_ranges = {};
    Vector<D3D12BindInfo>           bindings       = {};
    uint                            num_defaults   = 0;
    uint                            num_samplers   = 0;
    uint                            num_dynamics   = 0;
    D3D12_SHADER_VISIBILITY         visibility     = D3D12_SHADER_VISIBILITY_ALL;
    bool                            bindless       = false;

    // implementation in D3D12Layout.cpp
    explicit D3D12BindGroupLayout();
    explicit D3D12BindGroupLayout(const GPUBindGroupLayoutDescriptor& desc);

    void destroy();

    auto create(GPUBindGroupHeapHandle heap, const GPUBindGroupDescriptor& desc) -> D3D12BindGroup*;

    bool valid() const { return num_defaults + num_samplers + num_dynamics != 0; }

    // helper methods
    void copy_regular_descriptors(D3D12BindGroupHeap& heap, const GPUBindGroupEntry& entry, const D3D12BindInfo& bind_info, D3D12BindGroup& bind_group);
    void copy_sampler_descriptor(D3D12BindGroupHeap& heap, const GPUBindGroupEntry& entry, const D3D12BindInfo& bind_info, D3D12BindGroup& bind_group);
    void copy_texture_descriptor(D3D12BindGroupHeap& heap, const GPUBindGroupEntry& entry, const D3D12BindInfo& bind_info, D3D12BindGroup& bind_group);
    void create_buffer_descriptor(D3D12BindGroupHeap& heap, const GPUBindGroupEntry& entry, const D3D12BindInfo& bind_info, D3D12BindGroup& bind_group);
    void create_buffer_cbv_descriptor(D3D12BindGroupHeap& heap, const GPUBindGroupEntry& entry, const D3D12BindInfo& bind_info, D3D12BindGroup& bind_group);
    void create_buffer_uav_descriptor(D3D12BindGroupHeap& heap, const GPUBindGroupEntry& entry, const D3D12BindInfo& bind_info, D3D12BindGroup& bind_group);
    void create_bvh_descriptor(D3D12BindGroupHeap& heap, const GPUBindGroupEntry& entry, const D3D12BindInfo& bind_info, D3D12BindGroup& bind_group);
};

struct D3D12PipelineLayout
{
    ID3D12RootSignature* layout = nullptr;

    // regular bind groups
    Vector<D3D12BindGroupInfo> bindgroups = {};

    // push constant is treated differently
    uint push_constant_root_parameter = -1;

    // really awkward design, but I have no choice
    struct
    {
        ComPtr<ID3D12CommandSignature> dispatch_indirect;
        ComPtr<ID3D12CommandSignature> draw_indirect;
        ComPtr<ID3D12CommandSignature> draw_indexed_indirect;
    } signatures;

    // implementation in D3D12Layout.cpp
    explicit D3D12PipelineLayout();
    explicit D3D12PipelineLayout(const GPUPipelineLayoutDescriptor& desc);

    void destroy();

    bool valid() const { return layout != nullptr; }

    // helper methods
    void create_dispatch_indirect_signature();
    void create_draw_indirect_signature();
    void create_draw_indexed_indirect_signature();
};

struct D3D12Pipeline
{
    ID3D12PipelineState* pipeline = nullptr;

    GPUPipelineLayoutHandle layout; // D3D12Pipeline does NOT own this!

    D3D12_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;

    // D3D12 does not record the vertex buffer strides during pipelien creation
    Vector<uint> vertex_buffer_strides;

    // implementation in D3D12Pipeline.cpp
    explicit D3D12Pipeline();
    explicit D3D12Pipeline(const GPURenderPipelineDescriptor& desc);
    explicit D3D12Pipeline(const GPUComputePipelineDescriptor& desc);
    explicit D3D12Pipeline(const GPURayTracingPipelineDescriptor& desc);

    void destroy();

    bool valid() const { return pipeline != nullptr; }
};

struct D3D12Tlas
{
    ID3D12Resource*                                       tlas       = nullptr;
    D3D12MA::Allocation*                                  allocation = nullptr;
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO sizes      = {};
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS  build      = {};

    // used to build content of instances in memory
    D3D12Buffer storage;
    D3D12Buffer staging;
    D3D12Buffer instances;

    // other info
    uint             max_instance_count = 0;
    GPUBVHUpdateMode update_mode        = GPUBVHUpdateMode::BUILD;

    // implementation in D3D12Tlas.cpp
    explicit D3D12Tlas();
    explicit D3D12Tlas(const GPUTlasDescriptor& desc);

    void destroy();

    bool valid() const { return tlas != nullptr; }
};

struct D3D12Blas
{
    uint64_t                                              reference  = 0ull;
    ID3D12Resource*                                       blas       = nullptr;
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO sizes      = {};
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS  build      = {};
    Vector<D3D12_RAYTRACING_GEOMETRY_DESC>                geometries = {};

    // underlying storage for blas
    D3D12Buffer storage;

    // other info
    GPUBVHUpdateMode update_mode = GPUBVHUpdateMode::BUILD;

    // implementation in D3D12Blas.cpp
    explicit D3D12Blas();
    explicit D3D12Blas(const GPUBlasDescriptor& desc, GPUBlasGeometrySizeDescriptors sizes);

    void destroy();

    bool valid() const { return blas != nullptr; }
};

struct D3D12QuerySet
{
    ID3D12QueryHeap* pool = nullptr;
    GPUQueryType     type = GPUQueryType::TIMESTAMP;

    D3D12Buffer buffer;

    // implementation in D3D12QuerySet.cpp
    explicit D3D12QuerySet();
    explicit D3D12QuerySet(const GPUQuerySetDescriptor& desc);

    void destroy();

    bool valid() const { return pool != nullptr || buffer.valid(); }
};

struct D3D12CommandBuffer
{
    ID3D12GraphicsCommandList* command_buffer    = nullptr;
    ID3D12CommandAllocator*    command_allocator = nullptr;
    ID3D12CommandQueue*        command_queue     = nullptr;

    D3D12QuerySet      query_set;
    Optional<uint32_t> query_index;

    struct PSOStatus
    {
        bool                 compute  = false;
        D3D12Pipeline*       pipeline = nullptr;
        D3D12PipelineLayout* layout   = nullptr;
    };

    struct FenceOps
    {
        ID3D12Fence* fence = nullptr;
        uint64_t     value = 0ull;
    };

    PSOStatus pso;

    Vector<FenceOps> wait_fences;
    Vector<FenceOps> signal_fences;

    // implementation in D3D12CommandBuffer.cpp
    void wait(const D3D12Fence& fence, GPUBarrierSyncFlags sync);
    void signal(const D3D12Fence& fence, GPUBarrierSyncFlags sync);
    void destroy();
    void reset();
    void submit();
    void begin();
    void end();
};

struct D3D12CommandPool
{
    ID3D12CommandAllocator* command_allocator = nullptr;
    D3D12_COMMAND_LIST_TYPE command_type;

    void init(D3D12_COMMAND_LIST_TYPE type);
    void reset();
    void destroy();
    auto allocate() -> ID3D12GraphicsCommandList*;
};

struct D3D12Frame
{
    struct CommandBuffer
    {
        GPUQueueType       type    = GPUQueueType::DEFAULT;
        bool               primary = true;
        bool               used    = false;
        D3D12CommandBuffer cmd;

        CommandBuffer()
        {
            // do nothing
        }

        void reset()
        {
            used = false;
            cmd.reset();
        }
    };

    // used to check command buffer usage,
    // command buffers are short-lived, only usable within the frame.
    // frame id must match D3D12Frame's id
    uint32_t frame_id = 0u;

    // D3D12Frame does NOT own this!!!
    GPUFenceHandle         image_available_fence;
    GPUFenceHandle         render_complete_fence;
    Vector<GPUFenceHandle> existing_fences;

    D3D12CommandPool bundle_command_pool;
    D3D12CommandPool compute_command_pool;
    D3D12CommandPool graphics_command_pool;
    D3D12CommandPool transfer_command_pool;

    // allocate command buffers
    Vector<CommandBuffer> allocated_command_buffers;

    // shortcut for cmd buffer
    auto& command(GPUCommandEncoderHandle handle)
    {
        return allocated_command_buffers.at(handle.value).cmd;
    }

    // impementation in D3D12Frame.cpp
    void init();
    void wait();
    void reset();
    void free();
    auto allocate(GPUQueueType type, bool primary) -> GPUCommandEncoderHandle;
    auto create(const GPUBindGroupDescriptor& desc) -> GPUBindGroupHandle;
    void destroy();
};

struct D3D12Swapchain
{
    struct Frame
    {
        GPUTextureHandle     texture;
        GPUTextureViewHandle view;

        // implementation in D3D12Swapchain.cpp
        void init(D3D12Swapchain* swapchain, uint backbuffer_index, uint width, uint height);
        void destroy();

        bool valid() const { return texture.valid(); }
    };

    // implementation in D3D12Pipeline.cpp
    explicit D3D12Swapchain();
    explicit D3D12Swapchain(const GPUSurfaceDescriptor& desc);

    bool valid() const { return swapchain != 0; }

    void recreate();
    void destroy();

    IDXGISwapChain3* swapchain = nullptr;

    // objects for swapchain recreation
    GPUSurfaceDescriptor desc     = {};
    GPUExtent2D          extent   = {};
    GPUTextureFormat     format   = GPUTextureFormat::BGRA8UNORM;
    DXGI_FORMAT          dxformat = DXGI_FORMAT_B8G8R8A8_UNORM;

    // similar to Vulkan's VkPresentModeKHR
    D3D12PresentMode present_mode = {};

    // swapchain images
    Vector<Frame> frames;

    // synchronization primitives
    Vector<GPUFenceHandle> image_available_fences;
    Vector<GPUFenceHandle> render_complete_fences;
};

struct D3D12RHI
{
    RHIFlags rhiflags = 0;

    ID3D12Debug1*       debug_control  = nullptr; // equivalent to VkDebugUtilsMessenger
    ID3D12DebugDevice*  debug_device   = nullptr;
    IDXGIFactory4*      factory        = nullptr; // equivalent to VkInstance
    IDXGIAdapter1*      adapter        = nullptr; // equivalent to VkPhysicalDevice
    ID3D12Device*       device         = nullptr; // equivalent to VkDevice
    D3D12MA::Allocator* allocator      = nullptr;
    ID3D12CommandQueue* transfer_queue = nullptr;
    ID3D12CommandQueue* graphics_queue = nullptr;
    ID3D12CommandQueue* compute_queue  = nullptr;

    // fence used to wait on queues for completion
    D3D12Fence idle_fence;

    // cpu heap objects
    D3D12ObjectHeap rtv_heap;
    D3D12ObjectHeap dsv_heap;
    D3D12ObjectHeap sampler_heap;
    D3D12ObjectHeap cbv_srv_uav_heap;

    // gpu heap objects
    D3D12BindGroupHeapAllocator gpu_default_heap;
    D3D12BindGroupHeapAllocator gpu_sampler_heap;

    // frame objects
    Vector<D3D12Frame> frames = {};

    // frame tracker
    uint current_frame_index = 0;
    uint current_image_index = 0;

    // swapchain tracker
    GPUSurfaceHandle surface_tracker;

    // collection of objects
    D3D12ResourceManager<D3D12Fence>           fences;
    D3D12ResourceManager<D3D12Buffer>          buffers;
    D3D12ResourceManager<D3D12Texture>         textures;
    D3D12ResourceManager<D3D12TextureView>     views;
    D3D12ResourceManager<D3D12Sampler>         samplers;
    D3D12ResourceManager<D3D12Shader>          shaders;
    D3D12ResourceManager<D3D12Tlas>            tlases;
    D3D12ResourceManager<D3D12Blas>            blases;
    D3D12ResourceManager<D3D12QuerySet>        query_sets;
    D3D12ResourceManager<D3D12Pipeline>        pipelines;
    D3D12ResourceManager<D3D12PipelineLayout>  pipeline_layouts;
    D3D12ResourceManager<D3D12BindGroupHeap>   bind_group_heaps;
    D3D12ResourceManager<D3D12BindGroupLayout> bind_group_layouts;
    D3D12ResourceManager<D3D12Swapchain>       swapchains;

    auto current_frame() -> D3D12Frame& { return frames.at(current_frame_index % frames.size()); }

    void wait_idle()
    {
        if (!idle_fence.valid()) return;

        idle_fence.signal(graphics_queue, idle_fence.target);
        idle_fence.wait();
        idle_fence.reset();

        idle_fence.signal(compute_queue, idle_fence.target);
        idle_fence.wait();
        idle_fence.reset();

        idle_fence.signal(transfer_queue, idle_fence.target);
        idle_fence.wait();
        idle_fence.reset();
    }
};

// These are the functions that implements the plugin.
namespace api
{
    // instance apis
    bool create_instance(const RHIDescriptor& desc);
    void delete_instance();

    // surface apis
    bool create_surface(GPUSurfaceHandle& surface, const GPUSurfaceDescriptor& desc);
    void delete_surface(GPUSurfaceHandle surface);
    bool get_surface_extent(GPUSurfaceHandle surface, GPUExtent2D& extent);
    bool get_surface_format(GPUSurfaceHandle surface, GPUTextureFormat& format);
    uint get_surface_frames(GPUSurfaceHandle surface);

    // adapter apis
    bool create_adapter(GPUAdapterProps& adapter, const GPUAdapterDescriptor& desc);
    void delete_adapter();

    // device apis
    bool create_device(const GPUDeviceDescriptor& desc);
    void delete_device();

    // fence apis
    bool create_fence(GPUFenceHandle& fence);
    void delete_fence(GPUFenceHandle fence);

    // buffer apis
    bool create_buffer(GPUBufferHandle& buffer, const GPUBufferDescriptor& desc);
    void delete_buffer(GPUBufferHandle buffer);
    void map_buffer(GPUBufferHandle buffer, GPUMapMode mode, GPUSize64 offset, GPUSize64 size);
    void unmap_buffer(GPUBufferHandle buffer);
    void get_mapped_range(GPUBufferHandle buffer, MappedBufferRange& range);
    void get_mapped_state(GPUBufferHandle buffer, GPUMapState& state);

    // sampler apis
    bool create_sampler(GPUSamplerHandle& sampler, const GPUSamplerDescriptor& desc);
    void delete_sampler(GPUSamplerHandle sampler);

    // texture apis
    bool create_texture(GPUTextureHandle& texture, const GPUTextureDescriptor& desc);
    void delete_texture(GPUTextureHandle texture);
    bool create_texture_view(GPUTextureViewHandle& view, GPUTextureHandle texture, const GPUTextureViewDescriptor& desc);
    void delete_texture_view(GPUTextureViewHandle view);

    // shader apis
    bool create_shader_module(GPUShaderModuleHandle& shader, const GPUShaderModuleDescriptor& desc);
    void delete_shader_module(GPUShaderModuleHandle shader);

    // bvh blas apis
    bool create_blas(GPUBlasHandle& blas, const GPUBlasDescriptor& descriptor, GPUBlasGeometrySizeDescriptors sizes);
    void delete_blas(GPUBlasHandle blas);
    bool get_blas_sizes(GPUBlasHandle blas, GPUBVHSizes& sizes);

    // bvh tlas apis
    bool create_tlas(GPUTlasHandle& tlas, const GPUTlasDescriptor& descriptor);
    void delete_tlas(GPUTlasHandle tlas);
    bool get_tlas_sizes(GPUTlasHandle tlas, GPUBVHSizes& sizes);

    // query set apis
    bool create_query_set(GPUQuerySetHandle& query_set, const GPUQuerySetDescriptor& descriptor);
    void delete_query_set(GPUQuerySetHandle query_set);

    // bind group layout apis
    bool create_bind_group_layout(GPUBindGroupLayoutHandle& handle, const GPUBindGroupLayoutDescriptor& desc);
    void delete_bind_group_layout(GPUBindGroupLayoutHandle handle);

    // bind group heap apis
    bool create_bind_group_heap(GPUBindGroupHeapHandle& handle, const GPUBindGroupHeapDescriptor& desc);
    void delete_bind_group_heap(GPUBindGroupHeapHandle handle);
    void reset_bind_group_heap(GPUBindGroupHeapHandle handle);

    // pipeline layout apis
    bool create_pipeline_layout(GPUPipelineLayoutHandle& layout, const GPUPipelineLayoutDescriptor& desc);
    void delete_pipeline_layout(GPUPipelineLayoutHandle layout);

    // pipeline apis
    bool create_render_pipeline(GPURenderPipelineHandle& handle, const GPURenderPipelineDescriptor& desc);
    void delete_render_pipeline(GPURenderPipelineHandle pipeline);
    bool create_compute_pipeline(GPUComputePipelineHandle& handle, const GPUComputePipelineDescriptor& desc);
    void delete_compute_pipeline(GPUComputePipelineHandle pipeline);
    bool create_raytracing_pipeline(GPURayTracingPipelineHandle& handle, const GPURayTracingPipelineDescriptor& desc);
    void delete_raytracing_pipeline(GPURayTracingPipelineHandle pipeline);

    // d3d12 frame logic
    void new_frame();
    void end_frame();

    // d3d12 swapchain
    bool acquire_next_frame(GPUSurfaceHandle surface, GPUTextureHandle& texture, GPUTextureViewHandle& view, GPUFenceHandle& image_available_fence, GPUFenceHandle& render_complete_fence, bool& suboptimal);
    bool present_curr_frame(GPUSurfaceHandle surface);

    // d3d12 desciprtor
    bool create_bind_group(GPUBindGroupHandle& bind_group, const GPUBindGroupDescriptor& desc);

    // command buffer
    bool create_command_buffer(GPUCommandEncoderHandle& cmdbuffer, const GPUCommandBufferDescriptor& descriptor);
    bool create_command_bundle(GPUCommandEncoderHandle& cmdbuffer, const GPUCommandBundleDescriptor& descriptor);
    bool submit_command_buffer(GPUCommandEncoderHandle cmdbuffer);

    // device/queue related
    void wait_idle();
    void wait_fence(GPUFenceHandle handle);

} // namespace api

// d3d12 command buffer recording
namespace cmd
{
    void insert_debug_marker(GPUCommandEncoderHandle cmdbuffer, CString marker_label);
    void push_debug_group(GPUCommandEncoderHandle cmdbuffer, CString group_label);
    void pop_debug_group(GPUCommandEncoderHandle cmdbuffer);
    void wait_fence(GPUCommandEncoderHandle cmdbuffer, GPUFenceHandle fence, GPUBarrierSyncFlags sync);
    void signal_fence(GPUCommandEncoderHandle cmdbuffer, GPUFenceHandle fence, GPUBarrierSyncFlags sync);
    void begin_render_pass(GPUCommandEncoderHandle cmdbuffer, const GPURenderPassDescriptor& descriptor);
    void end_render_pass(GPUCommandEncoderHandle cmdbuffer);
    void set_render_pipeline(GPUCommandEncoderHandle cmdbuffer, GPURenderPipelineHandle pipeline);
    void set_compute_pipeline(GPUCommandEncoderHandle cmdbuffer, GPUComputePipelineHandle pipeline);
    void set_raytracing_pipeline(GPUCommandEncoderHandle cmdbuffer, GPURayTracingPipelineHandle pipeline);
    void set_bind_group(GPUCommandEncoderHandle cmdbuffer, GPUIndex32 index, GPUBindGroupHandle bind_group, GPUBufferDynamicOffsets dynamic_offsets);
    void set_push_constants(GPUCommandEncoderHandle cmdbuffer, GPUShaderStageFlags visibility, uint offset, uint size, void* data);
    void set_index_buffer(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle buffer, GPUIndexFormat format, GPUSize64 offset, GPUSize64 size);
    void set_vertex_buffer(GPUCommandEncoderHandle cmdbuffer, GPUIndex32 slot, GPUBufferHandle buffer, GPUSize64 offset, GPUSize64 size);
    void draw(GPUCommandEncoderHandle cmdbuffer, GPUSize32 vertex_count, GPUSize32 instance_count, GPUSize32 first_vertex, GPUSize32 first_instance);
    void draw_indexed(GPUCommandEncoderHandle cmdbuffer, GPUSize32 index_count, GPUSize32 instance_count, GPUSize32 first_index, GPUSignedOffset32 base_vertex, GPUSize32 first_instance);
    void draw_indirect(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle indirect_buffer, GPUSize64 indirect_offset, GPUSize32 draw_count);
    void draw_indexed_indirect(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle indirect_buffer, GPUSize64 indirect_offset, GPUSize32 draw_count);
    void dispatch_workgroups(GPUCommandEncoderHandle cmdbuffer, GPUSize32 x, GPUSize32 y, GPUSize32 z);
    void dispatch_workgroups_indirect(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle indirect_buffer, GPUSize64 indirect_offset);
    void copy_buffer_to_buffer(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle source, GPUSize64 source_offset, GPUBufferHandle destination, GPUSize64 destination_offset, GPUSize64 size);
    void copy_buffer_to_texture(GPUCommandEncoderHandle cmdbuffer, const GPUTexelCopyBufferInfo& source, const GPUTexelCopyTextureInfo& destination, GPUExtent3D copy_size);
    void copy_texture_to_buffer(GPUCommandEncoderHandle cmdbuffer, const GPUTexelCopyTextureInfo& source, const GPUTexelCopyBufferInfo& destination, const GPUExtent3D& copy_size);
    void copy_texture_to_texture(GPUCommandEncoderHandle cmdbuffer, const GPUTexelCopyTextureInfo& source, const GPUTexelCopyTextureInfo& destination, const GPUExtent3D& copy_size);
    void clear_buffer(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle buffer, GPUSize64 offset, GPUSize64 size);
    void set_viewport(GPUCommandEncoderHandle cmdbuffer, float x, float y, float w, float h, float min_depth, float max_depth);
    void set_scissor_rect(GPUCommandEncoderHandle cmdbuffer, GPUIntegerCoordinate x, GPUIntegerCoordinate y, GPUIntegerCoordinate w, GPUIntegerCoordinate h);
    void set_blend_constant(GPUCommandEncoderHandle cmdbuffer, GPUColor color);
    void set_stencil_reference(GPUCommandEncoderHandle cmdbuffer, GPUStencilValue reference);
    void begin_occlusion_query(GPUCommandEncoderHandle cmdbuffer, GPUSize32 query_index);
    void end_occlusion_query(GPUCommandEncoderHandle cmdbuffer);
    void write_timestamp(GPUCommandEncoderHandle cmdbuffer, GPUQuerySetHandle query_set, GPUSize32 query_index);
    void write_blas_properties(GPUCommandEncoderHandle cmdbuffer, GPUQuerySetHandle query_set, GPUSize32 query_index, GPUBlasHandle blas);
    void resolve_query_set(GPUCommandEncoderHandle cmdbuffer, GPUQuerySetHandle query_set, GPUSize32 first_query, GPUSize32 query_count, GPUBufferHandle destination, GPUSize64 destination_offset);
    void memory_barrier(GPUCommandEncoderHandle cmdbuffer, GPUMemoryBarriers barriers);
    void buffer_barrier(GPUCommandEncoderHandle cmdbuffer, GPUBufferBarriers barriers);
    void texture_barrier(GPUCommandEncoderHandle cmdbuffer, GPUTextureBarriers barriers);
    void build_tlases(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle scratch_buffer, GPUTlasBuildEntries entries);
    void build_blases(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle scratch_buffer, GPUBlasBuildEntries entries);
} // namespace cmd

auto get_logger() -> Logger;

// d3d12 rhi
void set_rhi(D3D12RHI* instance);
auto get_rhi() -> D3D12RHI*;

// helpers
auto infer_present_mode(GPUPresentMode mode) -> D3D12PresentMode;
auto infer_heap_type(GPUBufferUsageFlags usages) -> D3D12_HEAP_TYPE;
auto infer_buffer_flags(GPUBufferUsageFlags usages) -> D3D12_RESOURCE_FLAGS;
auto infer_texture_flags(GPUTextureUsageFlags usages, GPUTextureFormat format) -> D3D12_RESOURCE_FLAGS;
auto infer_texture_format(GPUTextureFormat format) -> DXGI_FORMAT;
auto infer_topology_type(GPUPrimitiveTopology topology) -> D3D12_PRIMITIVE_TOPOLOGY_TYPE;
auto infer_topology(GPUPrimitiveTopology topology) -> D3D12_PRIMITIVE_TOPOLOGY;
auto infer_row_pitch(DXGI_FORMAT format, uint width, uint bytes_per_row) -> uint;
auto d3d12enum(GPUCompositeAlphaMode mode) -> DXGI_ALPHA_MODE;
auto d3d12enum(GPUPresentMode mode) -> DXGI_SWAP_EFFECT;
auto d3d12enum(GPUShaderStageFlags visbility) -> D3D12_SHADER_VISIBILITY;
auto d3d12enum(GPUCompareFunction compare, bool enable) -> D3D12_COMPARISON_FUNC;
auto d3d12enum(GPUFilterMode min, GPUFilterMode mag, GPUMipmapFilterMode mip) -> D3D12_FILTER;
auto d3d12enum(GPUAddressMode mode) -> D3D12_TEXTURE_ADDRESS_MODE;
auto d3d12enum(GPUVertexFormat format) -> DXGI_FORMAT;
auto d3d12enum(GPUVertexStepMode format) -> D3D12_INPUT_CLASSIFICATION;
auto d3d12enum(GPUCullMode cull) -> D3D12_CULL_MODE;
auto d3d12enum(GPUStencilOperation cull) -> D3D12_STENCIL_OP;
auto d3d12enum(GPUBlendOperation op) -> D3D12_BLEND_OP;
auto d3d12enum(GPUBlendFactor factor) -> D3D12_BLEND;
auto d3d12enum(GPUIndexFormat format) -> DXGI_FORMAT;
auto d3d12enum(GPUBarrierLayout layout) -> D3D12_BARRIER_LAYOUT;
auto d3d12enum(GPUBarrierSyncFlags sync) -> D3D12_BARRIER_SYNC;
auto d3d12enum(GPUBarrierAccessFlags access) -> D3D12_BARRIER_ACCESS;
auto d3d12enum(GPUBVHFlags flags) -> D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS;
auto d3d12enum(GPUBVHGeometryFlags flags) -> D3D12_RAYTRACING_GEOMETRY_FLAGS;
uint size_of(DXGI_FORMAT format);

template <typename T, typename Handle>
T& fetch_resource(D3D12ResourceManager<T>& manager, Handle handle)
{
    // check handle validity
    if (!handle.valid()) {
        get_logger()->error("Resource handle {} is invalid!", typeid(Handle).name());
        throw std::runtime_error("Resource handle is invalid!");
    }

    // check resource range
    if (!manager.range_check(handle.value)) {
        get_logger()->error("Resource handle {} with value={} access out of range!", Handle::type_name(), handle.value);
        throw std::runtime_error("Resource handle is accessing out of range!");
    }

    T& resource = manager.at(handle.value);
    if (!resource.valid()) {
        get_logger()->error("Resource handle {} with value={} has invalid object!", Handle::type_name(), handle.value);
        throw std::runtime_error("Resource handle references an invalid object!");
    }
    return resource;
}

template <typename T>
void print_refcnt(T* data)
{
    get_logger()->info("refcnt: {}", data->AddRef() - 1);
    data->Release();
}

// helper method for error checking
inline std::string get_hresult_message(HRESULT hr)
{
    std::error_code ec(hr, std::system_category());
    return ec.message();
}

// helper method for error checking
inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr)) {
        // set a breakpoint on this line to catch DirectX API errors
        auto err = get_hresult_message(hr);
        if (err.c_str())
            get_logger()->error("error: {}\n", err.c_str());

        switch (hr) {
            case E_OUTOFMEMORY:
                throw GPUOutOfMemoryError(err);
            case DXGI_ERROR_DEVICE_REMOVED:
            case DXGI_ERROR_DEVICE_RESET:
                throw GPUDeviceLostInfo(err);
            default:
                throw GPUInternalError(err);
        }
    }
}

#endif // LYRA_PLUGIN_D3D12_UTILS_H
