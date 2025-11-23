#ifndef LYRA_PLUGIN_VULKAN_VKUTILS_H
#define LYRA_PLUGIN_VULKAN_VKUTILS_H

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#ifdef __linux__
#define VK_USE_PLATFORM_X11_KHR
#endif

#ifdef __APPLE__
#define VK_USE_PLATFORM_MACOS_MVK
#define VK_USE_PLATFORM_METAL_EXT
#endif

#define VK_EXT_debug_utils
#include <volk.h>
#include <vk_mem_alloc.h>
#include <sstream>

#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Msgbox.h>
#include <Lyra/Common/Slotmap.h>
#include <Lyra/Common/Container.h>
#include <Lyra/Common/Compatibility.h>
#include <Lyra/Render/RHI/RHIAPI.h>
#include <Lyra/Render/RHI/RHIDescs.h>

using namespace lyra;

static const char* KHR_PORTABILITY_EXTENSION_NAME = "VK_KHR_portability_subset";
static const char* LUNARG_VALIDATION_LAYER_NAME   = "VK_LAYER_KHRONOS_validation";

template <typename T>
struct VulkanDestroyer
{
    void operator()(T& obj)
    {
        obj.destroy();
    }
};

template <typename T>
using VulkanResourceManager = Slotmap<T, VulkanDestroyer<T>>;

struct QueueFamilyIndices
{
    Optional<uint> graphics;
    Optional<uint> compute;
    Optional<uint> present;
    Optional<uint> transfer;
};

struct SwapchainSupportDetails
{
    VkSurfaceCapabilitiesKHR   capabilities = {};
    Vector<VkSurfaceFormatKHR> formats;
    Vector<VkPresentModeKHR>   present_modes;
};

struct VulkanBase
{
    VkStructureType sType;
    void*           pNext;
};

struct VulkanBuffer
{
    VkBuffer          buffer         = VK_NULL_HANDLE;
    VmaAllocation     allocation     = {};
    VmaAllocationInfo alloc_info     = {};
    VkDeviceAddress   device_address = 0ull;

    uint8_t* mapped_data = nullptr;
    uint64_t mapped_size = 0ull;

    // implementation in VkBuffer.cpp
    explicit VulkanBuffer();
    explicit VulkanBuffer(const GPUBufferDescriptor& desc, VkBufferUsageFlags additional_usages = 0);

    void map(GPUSize64 offset = 0, GPUSize64 size = 0);
    void unmap();
    bool mapped() const { return mapped_data != nullptr; }
    void destroy();

    bool valid() const { return buffer != VK_NULL_HANDLE; }

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

struct VulkanTextureView;
struct VulkanTexture
{
    VkImage            image = VK_NULL_HANDLE;
    VmaAllocation      allocation;
    VmaAllocationInfo  alloc_info = {};
    VkFormat           format     = VK_FORMAT_UNDEFINED;
    VkImageAspectFlags aspects    = 0;
    VkExtent2D         area       = {}; // only used for Render Area

    // implementation in VkImage.cpp
    explicit VulkanTexture();
    explicit VulkanTexture(const GPUTextureDescriptor& desc);

    void destroy();

    bool valid() const { return image != VK_NULL_HANDLE; }
};

struct VulkanTextureView
{
    VkImageView        view    = VK_NULL_HANDLE;
    VkExtent2D         area    = {}; // only used for Render Area
    VkImageAspectFlags aspects = 0;

    // implementation in VkImage.cpp
    explicit VulkanTextureView();
    explicit VulkanTextureView(const VulkanTexture& texture, const GPUTextureViewDescriptor& desc);

    void destroy();

    bool valid() const { return view != VK_NULL_HANDLE; }
};

struct VulkanSampler
{
    VkSampler sampler = VK_NULL_HANDLE;

    // implementation in VkSampler.cpp
    explicit VulkanSampler();
    explicit VulkanSampler(const GPUSamplerDescriptor& desc);

    void destroy();

    bool valid() const { return sampler != VK_NULL_HANDLE; }
};

struct VulkanFence
{
    VkFence fence = VK_NULL_HANDLE;

    // implementation in VkFence.cpp
    explicit VulkanFence();
    explicit VulkanFence(bool signaled);

    void wait(uint64_t timeout = UINT64_MAX);
    void reset();
    void destroy();

    bool valid() const { return fence != VK_NULL_HANDLE; }
};

struct VulkanSemaphore
{
    VkSemaphore semaphore = VK_NULL_HANDLE;

    // we will have to support binary semaphores in some minimal cases
    VkSemaphoreType type = VK_SEMAPHORE_TYPE_TIMELINE;

    // keep track of target value
    uint64_t target = 0ull;

    // implementation in VkSemaphore.cpp
    explicit VulkanSemaphore();
    explicit VulkanSemaphore(VkSemaphoreType type);

    void wait(uint64_t timeout = UINT64_MAX);
    void reset();
    bool ready();
    void signal(uint64_t value);
    void destroy();

    bool valid() const { return semaphore != VK_NULL_HANDLE; }
};

struct VulkanQuerySet
{
    VkQueryPool pool  = VK_NULL_HANDLE;
    VkQueryType type  = VK_QUERY_TYPE_TIMESTAMP;
    uint32_t    count = 0;

    // implementation in VkQuery.cpp
    explicit VulkanQuerySet();
    explicit VulkanQuerySet(const GPUQuerySetDescriptor& desc);

    void destroy();

    bool valid() const { return pool != VK_NULL_HANDLE; }
};

struct VulkanShader
{
    VkShaderModule module = VK_NULL_HANDLE;

    // implementation in VkShader.cpp
    explicit VulkanShader();
    explicit VulkanShader(const GPUShaderModuleDescriptor& desc);

    void destroy();

    bool valid() const { return module != VK_NULL_HANDLE; }
};

struct VulkanBindGroupLayout
{
    VkDescriptorSetLayout layout   = VK_NULL_HANDLE;
    bool                  bindless = false;

    Vector<VkDescriptorType> binding_types = {};

    // implementation in VkLayout.cpp
    explicit VulkanBindGroupLayout();
    explicit VulkanBindGroupLayout(const GPUBindGroupLayoutDescriptor& desc);

    void destroy();

    bool valid() const { return layout != VK_NULL_HANDLE; }
};

struct VulkanPipelineLayout
{
    VkPipelineLayout layout = VK_NULL_HANDLE;

    // implementation in VkLayout.cpp
    explicit VulkanPipelineLayout();
    explicit VulkanPipelineLayout(const GPUPipelineLayoutDescriptor& desc);

    void destroy();

    bool valid() const { return layout != VK_NULL_HANDLE; }
};

struct VulkanPipeline
{
    VkPipeline       pipeline = VK_NULL_HANDLE;
    VkPipelineCache  cache    = VK_NULL_HANDLE;
    VkPipelineLayout layout   = VK_NULL_HANDLE; // VulkanPipeline does NOT own this.

    // implementation in VkPipeline.cpp
    explicit VulkanPipeline();
    explicit VulkanPipeline(const GPURenderPipelineDescriptor& desc);
    explicit VulkanPipeline(const GPUComputePipelineDescriptor& desc);
    explicit VulkanPipeline(const GPURayTracingPipelineDescriptor& desc);

    void destroy();

    bool valid() const { return pipeline != VK_NULL_HANDLE; }
};

struct VulkanTlas
{
    VkAccelerationStructureKHR                  tlas     = VK_NULL_HANDLE;
    VkAccelerationStructureBuildGeometryInfoKHR build    = {};
    VkAccelerationStructureBuildSizesInfoKHR    sizes    = {};
    VkAccelerationStructureBuildRangeInfoKHR    range    = {};
    VkAccelerationStructureGeometryKHR          geometry = {};

    // used to build content of instances in memory
    VulkanBuffer storage;
    VulkanBuffer staging;
    VulkanBuffer instances;

    // other info
    uint             max_instance_count = 0;
    GPUBVHUpdateMode update_mode        = GPUBVHUpdateMode::BUILD;

    // implementation in VkTlas.cpp
    explicit VulkanTlas();
    explicit VulkanTlas(const GPUTlasDescriptor& desc);

    void destroy();

    bool valid() const { return tlas != VK_NULL_HANDLE; }
};

struct VulkanBlas
{
    uint64_t                                         reference  = 0ull;
    VkAccelerationStructureKHR                       blas       = VK_NULL_HANDLE;
    VkAccelerationStructureBuildGeometryInfoKHR      build      = {};
    VkAccelerationStructureBuildSizesInfoKHR         sizes      = {};
    Vector<VkAccelerationStructureBuildRangeInfoKHR> ranges     = {};
    Vector<VkAccelerationStructureGeometryKHR>       geometries = {};

    // underlying storage for blas
    VulkanBuffer storage;

    // other info
    GPUBVHUpdateMode update_mode = GPUBVHUpdateMode::BUILD;

    // implementation in VkBlas.cpp
    explicit VulkanBlas();
    explicit VulkanBlas(const GPUBlasDescriptor& desc, GPUBlasGeometrySizeDescriptors sizes);

    void destroy();

    bool valid() const { return blas != VK_NULL_HANDLE; }
};

struct VulkanDescriptorPool
{
    GPUBindGroupHeapDescriptor desc      = {};
    Vector<VkDescriptorPool>   pools     = {};
    Vector<uint32_t>           counts    = {};
    uint32_t                   poolindex = 0;

    explicit VulkanDescriptorPool() : desc({}) {}
    explicit VulkanDescriptorPool(const GPUBindGroupHeapDescriptor& desc) : desc(desc) {}

    // implementation in VkDescriptorPool.cpp
    void destroy();

    // for compliance with in-house slotmap
    bool valid() const { return true; }

    void reset();

    auto allocate(
        VkDescriptorSetLayout layout,
        uint                  set_count      = 1,
        uint                  bindless_count = 0) -> VkDescriptorSet;

    uint find_pool_index(uint index);
};

struct VulkanCommandBuffer
{
    // used to check vulkan buffer usage,
    // vulkan command buffers are short-lived, only usable within the frame.
    // frame id must match VulkanFrame's id
    uint32_t frame_id = 0u;

    // CPU/GPU synchronization
    VulkanFence fence;

    // Query sets
    VulkanQuerySet     query_set;
    Optional<uint32_t> query_index;

    VkQueue         command_queue  = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;

    // cache for pipeline
    VkPipeline          last_bound_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout    last_bound_layout   = VK_NULL_HANDLE;
    VkPipelineBindPoint last_bound_point    = VK_PIPELINE_BIND_POINT_COMPUTE;

    // GPU/GPU synchronization
    Vector<VkSemaphoreSubmitInfo> wait_semaphores   = {};
    Vector<VkSemaphoreSubmitInfo> signal_semaphores = {};

    // additional resources to be clean-up after command buffer is reset
    Vector<VulkanBuffer> temporary_buffers;

    // implementation in VkCommandBuffer.cpp
    void wait(const VulkanSemaphore& fence, GPUBarrierSyncFlags sync);
    void signal(const VulkanSemaphore& fence, GPUBarrierSyncFlags sync);
    void reset();
    void submit();
    void begin();
    void end();
};

struct VulkanCommandPool
{
    VkCommandPool command_pool = VK_NULL_HANDLE;

    // implementation in VkCommandPool.cpp
    void init(uint queue_family_index);
    void reset(bool free = false);
    void destroy();
    auto allocate(bool primary = true) -> VkCommandBuffer;

    struct AllocatedCommandBuffers
    {
        Vector<VkCommandBuffer> allocated;
        uint32_t                index;

        auto allocate(VkCommandPool pool, VkCommandBufferLevel level) -> VkCommandBuffer;
        void reset(VkCommandPool pool, bool free);
    };

    AllocatedCommandBuffers primary;
    AllocatedCommandBuffers secondary;
};

struct VulkanFrame
{
    // used to check command buffer usage,
    // command buffers are short-lived, only usable within the frame.
    // frame id must match VulkanFrame's id
    uint32_t frame_id = 0u;

    // NOTE: VulkanFrame does NOT own these synchronization primitives.
    // These should be copied from VulkanSwapchain::Frame when frame is selected.
    VulkanFence     inflight_fence; // only the most recent one
    GPUFenceHandle  image_available_semaphore;
    GPUFenceHandle  render_complete_semaphore;
    Vector<VkFence> existing_fences;

    VulkanCommandPool compute_command_pool;
    VulkanCommandPool graphics_command_pool;
    VulkanCommandPool transfer_command_pool;

    // allocate command buffers
    Vector<VulkanCommandBuffer> allocated_command_buffers;

    // shortcut for cmd buffer
    auto& command(GPUCommandEncoderHandle handle)
    {
        return allocated_command_buffers.at(handle.value);
    }

    // implementation in VkFrame.cpp
    void init();
    void wait();
    void reset();
    void free();
    auto allocate(GPUQueueType type, bool primary) -> GPUCommandEncoderHandle;
    void destroy();
};

struct VulkanSwapchain
{
    struct Frame
    {
        GPUTextureHandle     texture;
        GPUTextureViewHandle view;

        // implementation in vkSwapchain.cpp
        void init(VkImage image, VkFormat format, VkExtent2D extent);
        void destroy();
    };

    // descriptor
    GPUSurfaceDescriptor desc = {};

    // surface object
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    // swapchain object
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;

    // objects for swapchain recreation
    VkExtent2D      extent;
    VkFormat        format;
    VkColorSpaceKHR colorspace;

    // swapchain frames
    Vector<Frame> frames = {};

    // fence objects
    Vector<VulkanFence>    inflight_fences;
    Vector<GPUFenceHandle> image_available_semaphores;
    Vector<GPUFenceHandle> render_complete_semaphores;

    // implementation in VkSwapchain.cpp
    explicit VulkanSwapchain();
    explicit VulkanSwapchain(const GPUSurfaceDescriptor& desc, VkSurfaceKHR surface);

    void recreate();
    void destroy();

    bool valid() const { return swapchain != VK_NULL_HANDLE; }
};

struct VulkanRHI
{
    RHIFlags           rhiflags       = 0;
    VkInstance         instance       = VK_NULL_HANDLE;
    VkSurfaceKHR       surface        = VK_NULL_HANDLE;
    VkPhysicalDevice   adapter        = VK_NULL_HANDLE;
    VkDevice           device         = VK_NULL_HANDLE;
    VkQueue            transfer_queue = VK_NULL_HANDLE;
    VkQueue            graphics_queue = VK_NULL_HANDLE;
    VkQueue            compute_queue  = VK_NULL_HANDLE;
    VkQueue            present_queue  = VK_NULL_HANDLE;
    VolkDeviceTable    vtable         = {};
    VmaAllocator       alloc;
    QueueFamilyIndices queues;

    // additional properties
    VkPhysicalDeviceProperties  props  = {};
    VkPhysicalDeviceProperties2 props2 = {};

    // frame objects
    Vector<VulkanFrame> frames = {};

    // frame tracker
    uint current_frame_index = 0;
    uint current_image_index = 0;

    // swapchain tracker
    GPUSurfaceHandle surface_tracker;

    // collection of objects
    VulkanResourceManager<VulkanSwapchain>       swapchains;
    VulkanResourceManager<VulkanSemaphore>       fences;
    VulkanResourceManager<VulkanBuffer>          buffers;
    VulkanResourceManager<VulkanTexture>         textures;
    VulkanResourceManager<VulkanTextureView>     views;
    VulkanResourceManager<VulkanSampler>         samplers;
    VulkanResourceManager<VulkanShader>          shaders;
    VulkanResourceManager<VulkanTlas>            tlases;
    VulkanResourceManager<VulkanBlas>            blases;
    VulkanResourceManager<VulkanQuerySet>        query_sets;
    VulkanResourceManager<VulkanPipeline>        pipelines;
    VulkanResourceManager<VulkanPipelineLayout>  pipeline_layouts;
    VulkanResourceManager<VulkanDescriptorPool>  descriptor_pools;
    VulkanResourceManager<VulkanBindGroupLayout> bind_group_layouts;

    auto current_frame() -> VulkanFrame& { return frames.at(current_frame_index % frames.size()); }

    // debug label
    void set_debug_label(VkObjectType type, uint64_t handle, CString name)
    {
        static auto vkSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)
            vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT");

        auto name_info         = VkDebugUtilsObjectNameInfoEXT{};
        name_info.sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        name_info.objectType   = type;
        name_info.objectHandle = handle;
        name_info.pObjectName  = name;

        vkSetDebugUtilsObjectNameEXT(device, &name_info);
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
    bool create_adapter(GPUAdapterProps& adapter, const GPUAdapterDescriptor& descriptor);
    void delete_adapter();

    // device apis
    bool create_device(const GPUDeviceDescriptor& desc);
    void delete_device();

    // fence apis
    bool create_fence(GPUFenceHandle& fence);
    bool create_fence(GPUFenceHandle& fence, VkSemaphoreType type); // NOTE: not an API, just placed here for convenience
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
    void reset_bind_group_layout(GPUBindGroupLayoutHandle handle);

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

    // vulkan frame logic
    void new_frame();
    void end_frame();

    // vulkan swapchain
    bool acquire_next_frame(GPUSurfaceHandle surface, GPUTextureHandle& texture, GPUTextureViewHandle& view, GPUFenceHandle& image_available_fence, GPUFenceHandle& render_complete_fence, bool& suboptimal);
    bool present_curr_frame(GPUSurfaceHandle surface);

    // vulkan desciprtor
    bool create_bind_group(GPUBindGroupHandle& bind_group, const GPUBindGroupDescriptor& desc);
    bool create_bind_group_heap(GPUBindGroupHeapHandle& heap, const GPUBindGroupHeapDescriptor& desc);
    void delete_bind_group_heap(GPUBindGroupHeapHandle heap);
    void reset_bind_group_heap(GPUBindGroupHeapHandle heap);

    // command buffer
    bool create_command_buffer(GPUCommandEncoderHandle& cmdbuffer, const GPUCommandBufferDescriptor& descriptor);
    bool create_command_bundle(GPUCommandEncoderHandle& cmdbuffer, const GPUCommandBundleDescriptor& descriptor);
    bool submit_command_buffer(GPUCommandEncoderHandle cmdbuffer);

    // device/queue related
    void wait_idle();
    void wait_fence(GPUFenceHandle handle);

} // namespace api

// vulkan command buffer recording
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

// to_string
auto to_string(VkResult result) -> CString;
auto to_string(VkDebugUtilsMessageTypeFlagsEXT type) -> CString;
auto to_string(VkDebugUtilsMessageSeverityFlagBitsEXT severity) -> CString;

// enum conversions
auto vkenum(GPUPresentMode mode) -> VkPresentModeKHR;
auto vkenum(GPUCompositeAlphaMode mode) -> VkCompositeAlphaFlagBitsKHR;
auto vkenum(GPUColorSpace space) -> VkColorSpaceKHR;
auto vkenum(GPUBlendOperation op) -> VkBlendOp;
auto vkenum(GPUBlendFactor factor) -> VkBlendFactor;
auto vkenum(GPULoadOp op) -> VkAttachmentLoadOp;
auto vkenum(GPUStoreOp op) -> VkAttachmentStoreOp;
auto vkenum(GPUQueryType query) -> VkQueryType;
auto vkenum(GPUTextureDimension dim) -> VkImageType;
auto vkenum(GPUTextureViewDimension dim) -> VkImageViewType;
auto vkenum(GPUAddressMode mode) -> VkSamplerAddressMode;
auto vkenum(GPUFilterMode filter) -> VkFilter;
auto vkenum(GPUMipmapFilterMode filter) -> VkSamplerMipmapMode;
auto vkenum(GPUCompareFunction op) -> VkCompareOp;
auto vkenum(GPUStencilOperation op) -> VkStencilOp;
auto vkenum(GPUFrontFace winding) -> VkFrontFace;
auto vkenum(GPUCullMode culling) -> VkCullModeFlagBits;
auto vkenum(GPUPrimitiveTopology topology) -> VkPrimitiveTopology;
auto vkenum(GPUVertexStepMode step) -> VkVertexInputRate;
auto vkenum(GPUIndexFormat format) -> VkIndexType;
auto vkenum(GPUVertexFormat format) -> VkFormat;
auto vkenum(GPUTextureFormat format) -> VkFormat;
auto vkenum(GPUBarrierLayout layout) -> VkImageLayout;
auto vkenum(GPUIntegerCoordinate samples) -> VkSampleCountFlagBits;
auto vkenum(GPUBlasType type) -> VkGeometryTypeKHR;
auto vkenum(GPUBVHUpdateMode mode) -> VkBuildAccelerationStructureModeKHR;
auto vkenum(GPUTextureAspectFlags aspect) -> VkImageAspectFlags;
auto vkenum(GPUColorWriteFlags color) -> VkColorComponentFlags;
auto vkenum(GPUBufferUsageFlags usages) -> VkBufferUsageFlags;
auto vkenum(GPUTextureUsageFlags usages, GPUTextureFormat format) -> VkImageUsageFlags;
auto vkenum(GPUShaderStageFlags stages) -> VkShaderStageFlags;
auto vkenum(GPUBarrierSyncFlags flags) -> VkPipelineStageFlags2;
auto vkenum(GPUBarrierAccessFlags flags) -> VkAccessFlags2;
auto vkenum(GPUBVHFlags flags) -> VkBuildAccelerationStructureFlagsKHR;
auto vkenum(GPUBVHGeometryFlags flags) -> VkGeometryFlagsKHR;

// vulkan rhi
void set_rhi(VulkanRHI* instance);
auto get_rhi() -> VulkanRHI*;

// vulkan surface utils
void add_surface_extension(Vector<const char*>& instance_extensions);
auto create_surface(VkInstance instance, const WindowHandle& handle) -> VkSurfaceKHR;

// vulkan buffer utils
auto get_buffer_device_address(VkBuffer buffer) -> VkDeviceAddress;

// vulkan descriptor pool
auto create_bind_group(const GPUBindGroupDescriptor& desc) -> GPUBindGroupHandle;
auto create_descriptor_pool(uint max_sets) -> VkDescriptorPool;
void reset_descriptor_pool(VkDescriptorPool pool);
void delete_descriptor_pool(VkDescriptorPool pool);

// adapter/device utils
bool has_portability_subset(VkPhysicalDevice physicalDevice);
auto get_supported_instance_extensions() -> HashSet<String>;
auto get_supported_device_extensions(VkPhysicalDevice device) -> HashSet<String>;
auto find_queue_family_indices(VkPhysicalDevice device, VkSurfaceKHR surface) -> QueueFamilyIndices;

// swaphain utils
auto query_swapchain_support(VkPhysicalDevice adapter, VkSurfaceKHR surface) -> SwapchainSupportDetails;
auto choose_swap_surface_format(const Vector<VkSurfaceFormatKHR>& availableFormats) -> VkSurfaceFormatKHR;
auto choose_swap_present_mode(const Vector<VkPresentModeKHR>& availablePresentModes) -> VkPresentModeKHR;
auto choose_swap_extent(const GPUSurfaceDescriptor& desc, const VkSurfaceCapabilitiesKHR& capabilities) -> VkExtent2D;

// size of
uint size_of(VkFormat format);
uint infer_texture_row_length(VkFormat format, uint bytes_per_row);

template <typename T, typename Handle>
T& fetch_resource(VulkanResourceManager<T>& manager, Handle handle)
{
    // check handle validity
    if (!handle.valid()) {
        get_logger()->error("Resource handle {} is invalid!", typeid(Handle).name());
        exit(1);
    }

    // check resource range
    if (!manager.range_check(handle.value)) {
        get_logger()->error("Resource handle {} with value={} access out of range!", Handle::type_name(), handle.value);
        exit(1);
    }

    T& resource = manager.at(handle.value);
    if (!resource.valid()) {
        get_logger()->error("Resource handle {} with value={} has invalid object!", Handle::type_name(), handle.value);
        exit(1);
    }
    return resource;
}

inline VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT             messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void*                                       pUserData)
{
    std::stringstream ss;
    ss << "[" << to_string(messageType) << "]";
    ss << "[" << to_string(messageSeverity) << "] ";
    ss << pCallbackData->pMessage;
    get_logger()->warn(ss.str());

    (void)pUserData;
    return VK_FALSE;
}

// helper macro to check vulkan object creation result
#define vk_check(result)                                      \
    {                                                         \
        if (result != VK_SUCCESS) {                           \
            get_logger()->error("{}:{}", __FILE__, __LINE__); \
            get_logger()->error("{}", to_string(result));     \
            std::exit(1);                                     \
        }                                                     \
    }

#endif // LYRA_PLUGIN_VULKAN_VKUTILS_H
