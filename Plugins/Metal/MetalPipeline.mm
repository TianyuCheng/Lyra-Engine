#include "MetalUtils.h"

using namespace lyra;

MetalPipeline::MetalPipeline()
{
    // do nothing
}

MetalPipeline::MetalPipeline(const GPURenderPipelineDescriptor& desc)
{
    auto rhi = get_rhi();

    // detect depth/stencil
    bool depth_enabled = desc.depth_stencil.depth_write_enabled || desc.depth_stencil.depth_compare != GPUCompareFunction::ALWAYS;
    bool stencil_test_enabled =
        desc.depth_stencil.stencil_front.compare != GPUCompareFunction::ALWAYS ||
        desc.depth_stencil.stencil_front.pass_op != GPUStencilOperation::KEEP ||
        desc.depth_stencil.stencil_front.fail_op != GPUStencilOperation::KEEP ||
        desc.depth_stencil.stencil_back.compare != GPUCompareFunction::ALWAYS ||
        desc.depth_stencil.stencil_back.pass_op != GPUStencilOperation::KEEP ||
        desc.depth_stencil.stencil_back.fail_op != GPUStencilOperation::KEEP;

    // store layout handle
    layout = desc.layout;

    // create render pipeline descriptor
    MTLRenderPipelineDescriptor* mtl_desc = [MTLRenderPipelineDescriptor new];

    // vertex function
    auto&     vshader       = fetch_resource(rhi->shaders, desc.vertex.module);
    NSString* vs_entry_ns   = [NSString stringWithUTF8String:desc.vertex.entry_point];
    mtl_desc.vertexFunction = [vshader.library newFunctionWithName:vs_entry_ns];

    if (!mtl_desc.vertexFunction) {
        get_logger()->error("FATAL: Vertex function with entry point '{}' not found in shader library.", desc.vertex.entry_point);
        render_pso = nil; // mark pipeline as invalid
        return;           // exit constructor early
    }

    // fragment function (optional)
    if (desc.fragment.module.valid()) {
        auto&     fshader         = fetch_resource(rhi->shaders, desc.fragment.module);
        NSString* fs_entry_ns     = [NSString stringWithUTF8String:desc.fragment.entry_point];
        mtl_desc.fragmentFunction = [fshader.library newFunctionWithName:fs_entry_ns];

        if (!mtl_desc.fragmentFunction) {
            get_logger()->error("FATAL: Fragment function with entry point '{}' not found in shader library.", desc.vertex.entry_point);
            render_pso = nil; // mark pipeline as invalid
            return;           // exit constructor early
        }
    }

    // vertex descriptor for vertex attributes
    if (!desc.vertex.buffers.empty()) {
        MTLVertexDescriptor* vertex_desc = [MTLVertexDescriptor new];

        uint binding = 0;
        for (auto& buffer_layout : desc.vertex.buffers) {
            uint32_t metal_binding = (METAL_PushConstantBufferIndex - 1) - binding;

            // Buffer layout
            vertex_desc.layouts[metal_binding].stride       = buffer_layout.array_stride;
            vertex_desc.layouts[metal_binding].stepRate     = 1;
            vertex_desc.layouts[metal_binding].stepFunction = mtlenum(buffer_layout.step_mode);

            // Attributes
            for (auto& attrib : buffer_layout.attributes) {
                vertex_desc.attributes[attrib.shader_location].format      = mtlenum(attrib.format);
                vertex_desc.attributes[attrib.shader_location].offset      = attrib.offset;
                vertex_desc.attributes[attrib.shader_location].bufferIndex = metal_binding;
            }
            binding++;
        }
        mtl_desc.vertexDescriptor = vertex_desc;
    }

    // color attachments
    uint color_index = 0;
    for (auto& target : desc.fragment.targets) {
        mtl_desc.colorAttachments[color_index].pixelFormat = mtlenum(target.format);
        mtl_desc.colorAttachments[color_index].writeMask   = mtlenum(target.write_mask);

        if (target.blend_enable) {
            mtl_desc.colorAttachments[color_index].blendingEnabled             = YES;
            mtl_desc.colorAttachments[color_index].rgbBlendOperation           = mtlenum(target.blend.color.operation);
            mtl_desc.colorAttachments[color_index].alphaBlendOperation         = mtlenum(target.blend.alpha.operation);
            mtl_desc.colorAttachments[color_index].sourceRGBBlendFactor        = mtlenum(target.blend.color.src_factor);
            mtl_desc.colorAttachments[color_index].destinationRGBBlendFactor   = mtlenum(target.blend.color.dst_factor);
            mtl_desc.colorAttachments[color_index].sourceAlphaBlendFactor      = mtlenum(target.blend.alpha.src_factor);
            mtl_desc.colorAttachments[color_index].destinationAlphaBlendFactor = mtlenum(target.blend.alpha.dst_factor);
        }
        color_index++;
    }

    // depth attachment format
    if (depth_enabled) {
        mtl_desc.depthAttachmentPixelFormat = mtlenum(desc.depth_stencil.format);

        // check if format has stencil component
        if (desc.depth_stencil.format == GPUTextureFormat::DEPTH24PLUS_STENCIL8 ||
            desc.depth_stencil.format == GPUTextureFormat::DEPTH32FLOAT_STENCIL8) {
            mtl_desc.stencilAttachmentPixelFormat = mtlenum(desc.depth_stencil.format);
        }
    }

    // multisample state
    mtl_desc.rasterSampleCount      = desc.multisample.count;
    mtl_desc.alphaToCoverageEnabled = desc.multisample.alpha_to_coverage_enabled;
    mtl_desc.alphaToOneEnabled      = desc.multisample.alpha_to_one_enabled;

    // store primitive state
    primitive_type = mtlenum(desc.primitive.topology);
    cull_mode      = mtlenum(desc.primitive.cull_mode);
    front_face     = mtlenum(desc.primitive.front_face);

    // store depth bias
    depth_bias       = desc.depth_stencil.depth_bias_constant;
    depth_bias_slope = desc.depth_stencil.depth_bias_slope_scale;
    depth_bias_clamp = desc.depth_stencil.depth_bias_clamp;

    // create render pipeline state
    NSError* error = nil;
    render_pso     = [rhi->device newRenderPipelineStateWithDescriptor:mtl_desc error:&error];
    if (error) {
        get_logger()->error("Failed to create render pipeline: {}", [[error localizedDescription] UTF8String]);
        return;
    }

    // create depth stencil state (separate from pipeline in Metal)
    if (depth_enabled) {
        MTLDepthStencilDescriptor* ds_desc = [MTLDepthStencilDescriptor new];

        bool depth_test_enabled = desc.depth_stencil.depth_compare != GPUCompareFunction::ALWAYS ||
                                  desc.depth_stencil.depth_write_enabled;

        ds_desc.depthCompareFunction = depth_test_enabled ? mtlenum(desc.depth_stencil.depth_compare) : MTLCompareFunctionAlways;
        ds_desc.depthWriteEnabled    = desc.depth_stencil.depth_write_enabled;

        if (stencil_test_enabled) {
            MTLStencilDescriptor* front_stencil     = [MTLStencilDescriptor new];
            front_stencil.stencilCompareFunction    = mtlenum(desc.depth_stencil.stencil_front.compare);
            front_stencil.stencilFailureOperation   = mtlenum(desc.depth_stencil.stencil_front.fail_op);
            front_stencil.depthFailureOperation     = mtlenum(desc.depth_stencil.stencil_front.depth_fail_op);
            front_stencil.depthStencilPassOperation = mtlenum(desc.depth_stencil.stencil_front.pass_op);
            front_stencil.readMask                  = desc.depth_stencil.stencil_read_mask;
            front_stencil.writeMask                 = desc.depth_stencil.stencil_write_mask;
            ds_desc.frontFaceStencil                = front_stencil;

            MTLStencilDescriptor* back_stencil     = [MTLStencilDescriptor new];
            back_stencil.stencilCompareFunction    = mtlenum(desc.depth_stencil.stencil_back.compare);
            back_stencil.stencilFailureOperation   = mtlenum(desc.depth_stencil.stencil_back.fail_op);
            back_stencil.depthFailureOperation     = mtlenum(desc.depth_stencil.stencil_back.depth_fail_op);
            back_stencil.depthStencilPassOperation = mtlenum(desc.depth_stencil.stencil_back.pass_op);
            back_stencil.readMask                  = desc.depth_stencil.stencil_read_mask;
            back_stencil.writeMask                 = desc.depth_stencil.stencil_write_mask;
            ds_desc.backFaceStencil                = back_stencil;
        }

        depth_stencil_state = [rhi->device newDepthStencilStateWithDescriptor:ds_desc];
    }

    // debug label
    if (desc.label && render_pso) {
        rhi->set_debug_label(render_pso, desc.label);
    }
}

MetalPipeline::MetalPipeline(const GPUComputePipelineDescriptor& desc)
{
    auto rhi = get_rhi();

    // store layout handle
    layout = desc.layout;

    // get shader
    auto& shader = fetch_resource(rhi->shaders, desc.compute.module);

    // get compute function
    NSString*       entry    = [NSString stringWithUTF8String:desc.compute.entry_point];
    id<MTLFunction> function = [shader.library newFunctionWithName:entry];
    if (!function) {
        get_logger()->error("Failed to find compute function: {}", desc.compute.entry_point);
        return;
    }

    // create compute pipeline state
    NSError* error = nil;
    compute_pso    = [rhi->device newComputePipelineStateWithFunction:function error:&error];
    if (error) {
        get_logger()->error("Failed to create compute pipeline: {}", [[error localizedDescription] UTF8String]);
        return;
    }

    // debug label
    if (desc.label && compute_pso) {
        rhi->set_debug_label(compute_pso, desc.label);
    }
}

MetalPipeline::MetalPipeline(const GPURayTracingPipelineDescriptor& desc)
{
    auto rhi = get_rhi();

    // check if device supports ray tracing
    if (![rhi->device supportsRaytracing]) {
        get_logger()->error("Metal ray tracing is not supported on this device");
        return;
    }

    // store layout handle
    layout = desc.layout;

    // store max recursion depth
    max_recursion_depth = desc.max_recursion_depth;

    // NOTE: Metal's ray tracing model differs from DXR/Vulkan RT extensions.
    // Metal uses:
    // 1. Compute pipelines that perform ray tracing via intersect() calls
    // 2. Visible function tables for callable/miss/hit shaders
    // 3. Intersection function tables for custom intersection tests
    //
    // The current GPURayTracingPipelineDescriptor only specifies max_recursion_depth.
    // To properly implement ray tracing pipelines, the RHI would need to be extended with:
    // - Ray generation shader stage
    // - Miss shader stages
    // - Hit group definitions (closest hit, any hit, intersection shaders)
    //
    // For now, this creates a valid but minimal ray tracing pipeline placeholder.
    // Users should use compute pipelines with inline ray tracing (ray queries) for
    // cross-platform compatibility until the RHI is extended.

    get_logger()->info("Created ray tracing pipeline placeholder with max_recursion_depth={}", max_recursion_depth);

    // To create actual function tables, you would need:
    // 1. A compute pipeline with linked functions for ray tracing
    // 2. MTLVisibleFunctionTableDescriptor to configure the visible function table
    // 3. MTLIntersectionFunctionTableDescriptor for custom intersection functions
    //
    // Example (requires shader modules with ray tracing functions):
    // MTLVisibleFunctionTableDescriptor* vft_desc = [MTLVisibleFunctionTableDescriptor new];
    // vft_desc.functionCount = <number of visible functions>;
    // visible_function_table = [compute_pso newVisibleFunctionTableWithDescriptor:vft_desc];
}

void MetalPipeline::destroy()
{
    render_pso                  = nil;
    compute_pso                 = nil;
    depth_stencil_state         = nil;
    visible_function_table      = nil;
    intersection_function_table = nil;
}

bool api::create_render_pipeline(GPURenderPipelineHandle& handle, const GPURenderPipelineDescriptor& desc)
{
    auto rhi = get_rhi();
    auto obj = MetalPipeline(desc);
    if (!obj.valid()) {
        return false;
    }
    auto ind = rhi->pipelines.add(obj);
    handle   = GPURenderPipelineHandle(ind);
    return true;
}

void api::delete_render_pipeline(GPURenderPipelineHandle handle)
{
    get_rhi()->pipelines.remove(handle.value);
}

bool api::create_compute_pipeline(GPUComputePipelineHandle& handle, const GPUComputePipelineDescriptor& desc)
{
    auto rhi = get_rhi();
    auto obj = MetalPipeline(desc);
    if (!obj.valid()) {
        return false;
    }
    auto ind = rhi->pipelines.add(obj);
    handle   = GPUComputePipelineHandle(ind);
    return true;
}

void api::delete_compute_pipeline(GPUComputePipelineHandle handle)
{
    get_rhi()->pipelines.remove(handle.value);
}

bool api::create_raytracing_pipeline(GPURayTracingPipelineHandle& handle, const GPURayTracingPipelineDescriptor& desc)
{
    auto rhi = get_rhi();

    // check device support
    if (![rhi->device supportsRaytracing]) {
        get_logger()->error("Metal ray tracing is not supported on this device");
        return false;
    }

    auto obj = MetalPipeline(desc);
    // Note: valid() check will pass even with just max_recursion_depth set
    // since the current implementation is a placeholder
    auto ind = rhi->pipelines.add(obj);
    handle   = GPURayTracingPipelineHandle(ind);
    return true;
}

void api::delete_raytracing_pipeline(GPURayTracingPipelineHandle handle)
{
    get_rhi()->pipelines.remove(handle.value);
}
