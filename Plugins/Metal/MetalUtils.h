#ifndef LYRA_PLUGIN_METAL_UTILS_H
#define LYRA_PLUGIN_METAL_UTILS_H

// platform detection
#ifdef __APPLE__
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#import <QuartzCore/CAMetalLayer.h>
#endif

#ifdef TARGET_OS_OSX
#import <AppKit/AppKit.h>
#endif

#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Function.h>
#include <Lyra/Common/Conversion.h>
#include <Lyra/Common/Collections.h>
#include <Lyra/Common/Compatibility.h>
#include <Lyra/Plugin/RHI/RHIAPI.h>
#include <Lyra/Plugin/RHI/RHIDescs.h>
#include <Lyra/Plugin/WSI/WSIAPI.h>
#include <Lyra/Plugin/WSI/WSIUtils.h>
#include <Lyra/Plugin/WSI/WSITypes.h>

using namespace lyra;

// resource manager pattern (from Vulkan backend)
template <typename T>
struct MetalDestroyer
{
    void operator()(T& obj)
    {
        obj.destroy();
    }
};

template <typename T>
using MetalResourceManager = Slotmap<T, MetalDestroyer<T>>;

// forward declarations
struct MetalBuffer;
struct MetalTexture;
struct MetalTextureView;
struct MetalSampler;
struct MetalFence;
struct MetalSemaphore;
struct MetalQuerySet;
struct MetalShader;
struct MetalBindGroupLayout;
struct MetalPipelineLayout;
struct MetalPipeline;
struct MetalTlas;
struct MetalBlas;
struct MetalBindGroup;
struct MetalBindGroupHeap;
struct MetalCommandBuffer;
struct MetalCommandPool;
struct MetalFrame;
struct MetalSwapchain;
struct MetalRHI;

// buffer
struct MetalBuffer
{
    id<MTLBuffer>      buffer       = nil;
    MTLResourceOptions storage_mode = MTLResourceStorageModeShared;
    uint8_t*           mapped_data  = nullptr;
    uint64_t           mapped_size  = 0ull;

    // implementation in MetalBuffer.mm
    explicit MetalBuffer();
    explicit MetalBuffer(const GPUBufferDescriptor& desc);

    void map(GPUSize64 offset = 0, GPUSize64 size = 0);
    void unmap();
    bool mapped() const { return mapped_data != nullptr; }
    void destroy();
    bool valid() const { return buffer != nil; }

    template <typename T>
    auto get_mapped_data_typed() -> T*
    {
        return reinterpret_cast<T*>(mapped_data);
    }
};

// texture
struct MetalTexture
{
    id<MTLTexture> texture = nil;
    MTLPixelFormat format  = MTLPixelFormatInvalid;
    MTLTextureType type    = MTLTextureType2D;

    // implementation in MetalTexture.mm
    explicit MetalTexture();
    explicit MetalTexture(const GPUTextureDescriptor& desc);

    void destroy();
    bool valid() const { return texture != nil; }
};

// texture view
struct MetalTextureView
{
    id<MTLTexture> texture = nil; // View references the parent texture
    MTLPixelFormat format  = MTLPixelFormatInvalid;
    MTLTextureType type    = MTLTextureType2D;

    // implementation in MetalTexture.mm
    explicit MetalTextureView();
    explicit MetalTextureView(const MetalTexture& parent_texture, const GPUTextureViewDescriptor& desc);

    void destroy();
    bool valid() const { return texture != nil; }
};

// sampler
struct MetalSampler
{
    id<MTLSamplerState> sampler = nil;

    // implementation in MetalSampler.mm
    explicit MetalSampler();
    explicit MetalSampler(const GPUSamplerDescriptor& desc);

    void destroy();
    bool valid() const { return sampler != nil; }
};

// fence/semaphore (using MTLSharedEvent for timeline semantics)
struct MetalFence
{
    id<MTLSharedEvent> event  = nil;
    mutable uint64_t   target = 0ull;

    // implementation in MetalFence.mm
    explicit MetalFence();
    explicit MetalFence(bool signaled);

    void wait(uint64_t timeout = UINT64_MAX);
    void signal(uint64_t value);
    void signal(id<MTLCommandBuffer> cmdbuf, uint64_t value);
    void destroy();
    bool valid() const { return event != nil; }
};

// query set
struct MetalQuerySet
{
    id<MTLCounterSampleBuffer> sample_buffer     = nil;
    id<MTLBuffer>              visibility_buffer = nil; // For occlusion queries
    GPUQueryType               type              = GPUQueryType::TIMESTAMP;
    uint32_t                   count             = 0;

    // implementation in MetalQuery.mm
    explicit MetalQuerySet();
    explicit MetalQuerySet(const GPUQuerySetDescriptor& desc);

    void destroy();
    bool valid() const { return sample_buffer != nil || visibility_buffer != nil; }
};

// shader module
struct MetalShader
{
    id<MTLLibrary> library = nil;

    // implementation in MetalShader.mm
    explicit MetalShader();
    explicit MetalShader(const GPUShaderModuleDescriptor& desc);

    void destroy();
    bool valid() const { return library != nil; }
};

// bind group (replaces argument buffer implementation)
struct MetalBindGroup
{
    struct Entry
    {
        uint32_t        binding;
        GPUResourceType type;
        union
        {
            struct
            {
                __unsafe_unretained id<MTLBuffer> buffer;
                NSUInteger                        offset;
            } buffer;
            struct
            {
                __unsafe_unretained id<MTLTexture> texture;
            } texture;
            struct
            {
                __unsafe_unretained id<MTLSamplerState> sampler;
            } sampler;
            struct
            {
                __unsafe_unretained id<MTLAccelerationStructure> tlas;
            } tlas;
        };
    };

    Entry*                 entries     = nullptr;
    uint32_t               entry_count = 0;
    GPUBindGroupHeapHandle heap;

    // implementation in MetalBindGroup.mm
    explicit MetalBindGroup();
    explicit MetalBindGroup(const GPUBindGroupDescriptor& desc);

    void destroy() {} // No-op for O(1) reset
    bool valid() const { return entries != nullptr; }
};

// bind group layout
struct MetalBindGroupLayout
{
    Vector<GPUBindGroupLayoutEntry> entries;

    // implementation in MetalLayout.mm
    explicit MetalBindGroupLayout();
    explicit MetalBindGroupLayout(const GPUBindGroupLayoutDescriptor& desc);

    void destroy();
    bool valid() const { return !entries.empty(); }
};

// pipeline layout
struct MetalPipelineLayout
{
    Vector<GPUBindGroupLayoutHandle> bind_group_layouts;
    Vector<GPUPushConstantRange>     push_constant_ranges;

    // Flat index mappings: (set << 16 | binding) -> metal_index
    std::unordered_map<uint32_t, uint32_t> buffer_indices;
    std::unordered_map<uint32_t, uint32_t> texture_indices;
    std::unordered_map<uint32_t, uint32_t> sampler_indices;

    // Max indices used (for collision detection)
    uint32_t max_buffer_index  = 0;
    uint32_t max_texture_index = 0;
    uint32_t max_sampler_index = 0;

    // implementation in MetalLayout.mm
    explicit MetalPipelineLayout();
    explicit MetalPipelineLayout(const GPUPipelineLayoutDescriptor& desc);

    void destroy();
    bool valid() const { return !bind_group_layouts.empty(); }
};

// pipeline (Render, Compute, RayTracing)
struct MetalPipeline
{
    id<MTLRenderPipelineState>  render_pso          = nil;
    id<MTLComputePipelineState> compute_pso         = nil;
    id<MTLDepthStencilState>    depth_stencil_state = nil;

    // Ray tracing specific (Metal uses visible/intersection function tables)
    id<MTLVisibleFunctionTable>      visible_function_table      = nil;
    id<MTLIntersectionFunctionTable> intersection_function_table = nil;
    uint                             max_recursion_depth         = 1;

    GPUPipelineLayoutHandle layout;
    MTLPrimitiveType        primitive_type = MTLPrimitiveTypeTriangle;
    MTLCullMode             cull_mode      = MTLCullModeNone;
    MTLWinding              front_face     = MTLWindingCounterClockwise;

    // Depth bias (not stored in PSO - must be set dynamically)
    float depth_bias       = 0.0f;
    float depth_bias_slope = 0.0f;
    float depth_bias_clamp = 0.0f;

    // implementation in MetalPipeline.mm
    explicit MetalPipeline();
    explicit MetalPipeline(const GPURenderPipelineDescriptor& desc);
    explicit MetalPipeline(const GPUComputePipelineDescriptor& desc);
    explicit MetalPipeline(const GPURayTracingPipelineDescriptor& desc);

    void destroy();
    bool valid() const { return render_pso != nil || compute_pso != nil || visible_function_table != nil; }
};

// acceleration Structures (TLAS)
struct MetalTlas
{
    id<MTLAccelerationStructure>        tlas       = nil;
    MTLAccelerationStructureDescriptor* descriptor = nil;
    MTLAccelerationStructureSizes       sizes      = {};

    // implementation in MetalTlas.mm
    explicit MetalTlas();
    explicit MetalTlas(const GPUTlasDescriptor& desc);

    void destroy();
    bool valid() const { return tlas != nil; }
};

// acceleration Structures (BLAS)
struct MetalBlas
{
    id<MTLAccelerationStructure>        blas       = nil;
    MTLAccelerationStructureDescriptor* descriptor = nil;
    MTLAccelerationStructureSizes       sizes      = {};

    // implementation in MetalBlas.mm
    explicit MetalBlas();
    explicit MetalBlas(const GPUBlasDescriptor& desc, GPUBlasGeometrySizeDescriptors size_descs);

    void destroy();
    bool valid() const { return blas != nil; }
};

#include <Lyra/Common/Memory.h>

// bind group heap
struct MetalBindGroupHeap
{
    Ref<lyra::MemoryArena> arena;

    // implementation in MetalBindGroup.mm
    explicit MetalBindGroupHeap();
    explicit MetalBindGroupHeap(const GPUBindGroupHeapDescriptor& desc);

    void* allocate(size_t size, size_t alignment);
    void  reset();
    void  destroy();
    bool  valid() const { return true; }
};

// command buffer
struct MetalCommandBuffer
{
    // frame tracking
    uint32_t frame_id = 0u;

    id<MTLCommandBuffer> command_buffer = nil;
    id<MTLCommandQueue>  command_queue  = nil;

    // encoder state machine (only ONE encoder can be active at a time)
    id<MTLRenderCommandEncoder>                render_encoder  = nil;
    id<MTLComputeCommandEncoder>               compute_encoder = nil;
    id<MTLBlitCommandEncoder>                  blit_encoder    = nil;
    id<MTLAccelerationStructureCommandEncoder> accel_encoder   = nil;

    enum EncoderType
    {
        NONE,
        RENDER,
        COMPUTE,
        BLIT,
        ACCEL
    };
    EncoderType active_encoder = NONE;

    // cached pipeline state
    id<MTLRenderPipelineState>  bound_render_pso          = nil;
    id<MTLComputePipelineState> bound_compute_pso         = nil;
    id<MTLDepthStencilState>    bound_depth_stencil_state = nil;
    GPUPipelineLayoutHandle     bound_layout;

    // cached buffer state
    id<MTLBuffer>  bound_index_buffer  = nil;
    GPUIndexFormat index_format        = GPUIndexFormat::UINT16;
    MTLIndexType   index_type          = MTLIndexTypeUInt16;
    uint64_t       index_buffer_offset = 0;

    // primitive type (cached from pipeline)
    MTLPrimitiveType primitive_type = MTLPrimitiveTypeTriangle;

    // synchronization
    MetalFence                 fence;
    Vector<id<MTLSharedEvent>> wait_events;
    Vector<uint64_t>           wait_values;
    Vector<id<MTLSharedEvent>> signal_events;
    Vector<uint64_t>           signal_values;

    // implementation in MetalCommandBuffer.mm
    void transition_encoder(EncoderType new_type);
    void end_current_encoder();
    void reset();
    void submit();
    void begin();
    void end();
};

// command pool (wraps MTLCommandQueue)
struct MetalCommandPool
{
    id<MTLCommandQueue> command_queue = nil;

    // implementation in MetalCommandPool.mm
    void init(id<MTLCommandQueue> queue);
    void destroy();
    auto allocate() -> id<MTLCommandBuffer>;
};

// frame (per-frame resource tracking)
struct MetalFrame
{
    uint32_t frame_id = 0u;

    // synchronization (NOT owned by frame, just references)
    MetalFence         inflight_fence;
    GPUFenceHandle     image_available_semaphore;
    GPUFenceHandle     render_complete_semaphore;
    Vector<MetalFence> existing_fences;

    // command pools
    MetalCommandPool compute_command_pool;
    MetalCommandPool graphics_command_pool;
    MetalCommandPool transfer_command_pool;

    // allocated command buffers
    Vector<MetalCommandBuffer> allocated_command_buffers;

    // shortcut for cmd buffer access
    auto& command(GPUCommandEncoderHandle handle)
    {
        return allocated_command_buffers.at(handle.value);
    }

    // implementation in MetalFrame.mm
    void init(id<MTLCommandQueue> graphics_queue, id<MTLCommandQueue> compute_queue, id<MTLCommandQueue> transfer_queue);
    void wait();
    void reset();
    void free();
    auto allocate(GPUQueueType type, bool primary) -> GPUCommandEncoderHandle;
    void destroy();
};

// swapchain
struct MetalSwapchain
{
    struct Frame
    {
        id<CAMetalDrawable>  drawable = nil;
        GPUTextureHandle     texture;
        GPUTextureViewHandle view;

        // implementation in MetalSwapchain.mm
        void init(id<CAMetalDrawable> drawable);
        void destroy();
    };

    CAMetalLayer*        metal_layer = nil;
    GPUSurfaceDescriptor desc        = {};

    // swapchain state
    GPUExtent2D      extent;
    MTLPixelFormat   format;
    GPUTextureFormat rhi_format;

    // frames
    Vector<Frame> frames;

    // fence objects
    Vector<MetalFence>     inflight_fences;
    Vector<GPUFenceHandle> image_available_semaphores;
    Vector<GPUFenceHandle> render_complete_semaphores;

    // implementation in MetalSwapchain.mm
    explicit MetalSwapchain();
    explicit MetalSwapchain(const GPUSurfaceDescriptor& desc);

    void recreate();
    void destroy();
    bool valid() const { return metal_layer != nil; }
};

// central RHI state manager
struct MetalRHI
{
    RHIFlags rhiflags = 0;

    // Metal device and queues
    id<MTLDevice>       device         = nil;
    id<MTLCommandQueue> graphics_queue = nil;
    id<MTLCommandQueue> compute_queue  = nil;
    id<MTLCommandQueue> transfer_queue = nil;

    // frame objects
    Vector<MetalFrame> frames = {};

    // frame tracker
    uint current_frame_index = 0;
    uint current_image_index = 0;

    // swapchain tracker
    GPUSurfaceHandle surface_tracker;

    // resource managers
    MetalResourceManager<MetalSwapchain>       swapchains;
    MetalResourceManager<MetalFence>           fences;
    MetalResourceManager<MetalBuffer>          buffers;
    MetalResourceManager<MetalTexture>         textures;
    MetalResourceManager<MetalTextureView>     views;
    MetalResourceManager<MetalSampler>         samplers;
    MetalResourceManager<MetalShader>          shaders;
    MetalResourceManager<MetalTlas>            tlases;
    MetalResourceManager<MetalBlas>            blases;
    MetalResourceManager<MetalQuerySet>        query_sets;
    MetalResourceManager<MetalPipeline>        pipelines;
    MetalResourceManager<MetalPipelineLayout>  pipeline_layouts;
    MetalResourceManager<MetalBindGroupHeap>   bind_group_heaps;
    MetalResourceManager<MetalBindGroupLayout> bind_group_layouts;

    auto current_frame() -> MetalFrame&
    {
        return frames.at(current_frame_index % frames.size());
    }

    void wait_idle();

    // debug label support (for MTLResource and other label-supporting objects)
    template <typename T>
    void set_debug_label(T object, CString name)
    {
        if (rhiflags.contains(RHIFlag::DEBUG)) {
            if ([object respondsToSelector:@selector(setLabel:)]) {
                @autoreleasepool {
                    [object performSelector:@selector(setLabel:)
                                 withObject:[NSString stringWithUTF8String:name]];
                }
            }
        }
    }
};

// API function declarations (follow RenderAPI interface)
namespace api
{
    // instance APIs
    bool create_instance(const RHIDescriptor& desc);
    void delete_instance();

    // surface APIs
    bool create_surface(GPUSurfaceHandle& surface, const GPUSurfaceDescriptor& desc);
    void delete_surface(GPUSurfaceHandle surface);
    bool get_surface_extent(GPUSurfaceHandle surface, GPUExtent2D& extent);
    bool get_surface_format(GPUSurfaceHandle surface, GPUTextureFormat& format);
    uint get_surface_frames(GPUSurfaceHandle surface);

    // adapter APIs
    bool create_adapter(GPUAdapterProps& adapter, const GPUAdapterDescriptor& descriptor);
    void delete_adapter();

    // device APIs
    bool create_device(const GPUDeviceDescriptor& desc);
    void delete_device();

    // fence APIs
    bool create_fence(GPUFenceHandle& fence);
    void delete_fence(GPUFenceHandle fence);

    // buffer APIs
    bool create_buffer(GPUBufferHandle& buffer, const GPUBufferDescriptor& desc);
    void delete_buffer(GPUBufferHandle buffer);
    void map_buffer(GPUBufferHandle buffer, GPUMapMode mode, GPUSize64 offset, GPUSize64 size);
    void unmap_buffer(GPUBufferHandle buffer);
    void get_mapped_range(GPUBufferHandle buffer, MappedBufferRange& range);
    void get_mapped_state(GPUBufferHandle buffer, GPUMapState& state);

    // sampler APIs
    bool create_sampler(GPUSamplerHandle& sampler, const GPUSamplerDescriptor& desc);
    void delete_sampler(GPUSamplerHandle sampler);

    // texture APIs
    bool create_texture(GPUTextureHandle& texture, const GPUTextureDescriptor& desc);
    void delete_texture(GPUTextureHandle texture);
    bool create_texture_view(GPUTextureViewHandle& view, GPUTextureHandle texture, const GPUTextureViewDescriptor& desc);
    void delete_texture_view(GPUTextureViewHandle view);

    // shader APIs
    bool create_shader_module(GPUShaderModuleHandle& shader, const GPUShaderModuleDescriptor& desc);
    void delete_shader_module(GPUShaderModuleHandle shader);

    // BVH BLAS APIs
    bool create_blas(GPUBlasHandle& blas, const GPUBlasDescriptor& descriptor, GPUBlasGeometrySizeDescriptors sizes);
    void delete_blas(GPUBlasHandle blas);
    bool get_blas_sizes(GPUBlasHandle blas, GPUBVHSizes& sizes);

    // BVH TLAS APIs
    bool create_tlas(GPUTlasHandle& tlas, const GPUTlasDescriptor& descriptor);
    void delete_tlas(GPUTlasHandle tlas);
    bool get_tlas_sizes(GPUTlasHandle tlas, GPUBVHSizes& sizes);

    // query set APIs
    bool create_query_set(GPUQuerySetHandle& query_set, const GPUQuerySetDescriptor& descriptor);
    void delete_query_set(GPUQuerySetHandle query_set);

    // bind group layout APIs
    bool create_bind_group_layout(GPUBindGroupLayoutHandle& handle, const GPUBindGroupLayoutDescriptor& desc);
    void delete_bind_group_layout(GPUBindGroupLayoutHandle handle);

    // pipeline layout APIs
    bool create_pipeline_layout(GPUPipelineLayoutHandle& layout, const GPUPipelineLayoutDescriptor& desc);
    void delete_pipeline_layout(GPUPipelineLayoutHandle layout);

    // pipeline APIs
    bool create_render_pipeline(GPURenderPipelineHandle& handle, const GPURenderPipelineDescriptor& desc);
    void delete_render_pipeline(GPURenderPipelineHandle pipeline);
    bool create_compute_pipeline(GPUComputePipelineHandle& handle, const GPUComputePipelineDescriptor& desc);
    void delete_compute_pipeline(GPUComputePipelineHandle pipeline);
    bool create_raytracing_pipeline(GPURayTracingPipelineHandle& handle, const GPURayTracingPipelineDescriptor& desc);
    void delete_raytracing_pipeline(GPURayTracingPipelineHandle pipeline);

    // frame management
    void new_frame();
    void end_frame();

    // swapchain API
    bool acquire_next_frame(GPUSurfaceHandle surface, GPUTextureHandle& texture, GPUTextureViewHandle& view,
        GPUFenceHandle& image_available_fence, GPUFenceHandle& render_complete_fence, bool& suboptimal);
    bool present_curr_frame(GPUSurfaceHandle surface);

    // bind group APIs
    bool create_bind_group(GPUBindGroupHandle& bind_group, const GPUBindGroupDescriptor& desc);
    bool create_bind_group_heap(GPUBindGroupHeapHandle& heap, const GPUBindGroupHeapDescriptor& desc);
    void delete_bind_group_heap(GPUBindGroupHeapHandle heap);
    void reset_bind_group_heap(GPUBindGroupHeapHandle heap);

    // command buffer APIS
    bool create_command_buffer(GPUCommandEncoderHandle& cmdbuffer, const GPUCommandBufferDescriptor& descriptor);
    bool create_command_bundle(GPUCommandEncoderHandle& cmdbuffer, const GPUCommandBundleDescriptor& descriptor);
    bool submit_command_buffer(GPUCommandEncoderHandle cmdbuffer);

    // device/queue related APIs
    void wait_idle();
    void wait_fence(GPUFenceHandle handle);

} // namespace api

// command buffer recording
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
    void clear_texture(GPUCommandEncoderHandle cmdbuffer, GPUTextureHandle texture, const GPUTextureSubresourceRange& range);
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
    void copy_blas(GPUCommandEncoderHandle cmdbuffer, GPUBlasHandle old_blas, GPUBlasHandle new_blas);

} // namespace cmd

// utility functions
auto get_logger() -> Logger;

// Enum conversions (mtlenum overloads)
auto mtlenum(GPUPresentMode mode) -> MTLPixelFormat;
auto mtlenum(GPUCompositeAlphaMode mode) -> uint32_t;
auto mtlenum(GPUColorSpace space) -> uint32_t;
auto mtlenum(GPUBlendOperation op) -> MTLBlendOperation;
auto mtlenum(GPUBlendFactor factor) -> MTLBlendFactor;
auto mtlenum(GPULoadOp op) -> MTLLoadAction;
auto mtlenum(GPUStoreOp op) -> MTLStoreAction;
auto mtlenum(GPUQueryType query) -> uint32_t;
auto mtlenum(GPUTextureDimension dim) -> MTLTextureType;
auto mtlenum(GPUTextureViewDimension dim) -> MTLTextureType;
auto mtlenum(GPUAddressMode mode) -> MTLSamplerAddressMode;
auto mtlenum(GPUFilterMode filter) -> MTLSamplerMinMagFilter;
auto mtlenum(GPUMipmapFilterMode filter) -> MTLSamplerMipFilter;
auto mtlenum(GPUCompareFunction op) -> MTLCompareFunction;
auto mtlenum(GPUStencilOperation op) -> MTLStencilOperation;
auto mtlenum(GPUFrontFace winding) -> MTLWinding;
auto mtlenum(GPUCullMode culling) -> MTLCullMode;
auto mtlenum(GPUPrimitiveTopology topology) -> MTLPrimitiveType;
auto mtlenum(GPUVertexStepMode step) -> MTLVertexStepFunction;
auto mtlenum(GPUIndexFormat format) -> MTLIndexType;
auto mtlenum(GPUVertexFormat format) -> MTLVertexFormat;
auto mtlenum(GPUTextureFormat format) -> MTLPixelFormat;
auto mtlenum(GPUBarrierLayout layout) -> uint32_t;
auto mtlenum(GPUIntegerCoordinate samples) -> NSUInteger;
auto mtlenum(GPUBlasType type) -> uint32_t;
auto mtlenum(GPUBVHUpdateMode mode) -> uint32_t;
auto mtlenum(GPUTextureAspectFlags aspect) -> MTLTextureUsage;
auto mtlenum(GPUColorWriteFlags color) -> MTLColorWriteMask;
auto mtlenum(GPUBufferUsageFlags usages) -> std::pair<MTLResourceOptions, MTLStorageMode>;
auto mtlenum(GPUTextureUsageFlags usages) -> MTLTextureUsage;
auto mtlenum(GPUShaderStageFlags stages) -> uint32_t;
auto mtlenum(GPUBarrierSyncFlags flags) -> MTLBarrierScope;
auto mtlenum(GPUBarrierAccessFlags flags) -> uint32_t;
auto mtlenum(GPUBVHFlags flags) -> uint32_t;
auto mtlenum(GPUBVHGeometryFlags flags) -> uint32_t;

// Metal RHI getters/setters
void set_rhi(MetalRHI* instance);
auto get_rhi() -> MetalRHI*;

// resource fetching template
template <typename T, typename Handle>
T& fetch_resource(MetalResourceManager<T>& manager, Handle handle)
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

#endif // LYRA_PLUGIN_METAL_UTILS_H
