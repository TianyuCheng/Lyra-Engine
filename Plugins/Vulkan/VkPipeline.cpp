#include "VkUtils.h"

VkPipelineShaderStageCreateInfo create_shader_stage(VulkanShader& shader, CString entry, VkShaderStageFlagBits stage)
{
    auto create_info   = VkPipelineShaderStageCreateInfo{};
    create_info.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    create_info.module = shader.module;
    create_info.pName  = entry;
    create_info.stage  = stage;
    return create_info;
}

VulkanPipeline::VulkanPipeline() : pipeline(VK_NULL_HANDLE), cache(VK_NULL_HANDLE)
{
    // do nothing
}

VulkanPipeline::VulkanPipeline(const GPURenderPipelineDescriptor& desc)
{
    auto rhi = get_rhi();

    auto& layout  = fetch_resource(rhi->pipeline_layouts, desc.layout);
    auto& vshader = fetch_resource(rhi->shaders, desc.vertex.module);
    auto& fshader = fetch_resource(rhi->shaders, desc.fragment.module);

    // shader stages
    Vector<VkPipelineShaderStageCreateInfo> stages;
    stages.push_back(create_shader_stage(vshader, desc.vertex.entry_point, VK_SHADER_STAGE_VERTEX_BIT));
    stages.push_back(create_shader_stage(fshader, desc.fragment.entry_point, VK_SHADER_STAGE_FRAGMENT_BIT));

    // vertex attributes
    uint                                      binding = 0;
    Vector<VkVertexInputBindingDescription>   vertex_layouts;
    Vector<VkVertexInputAttributeDescription> vertex_attribs;
    for (auto& layout : desc.vertex.buffers) {
        auto vertex_layout      = VkVertexInputBindingDescription{};
        vertex_layout.binding   = binding++;
        vertex_layout.inputRate = vkenum(layout.step_mode);
        vertex_layout.stride    = static_cast<uint32_t>(layout.array_stride);
        vertex_layouts.push_back(vertex_layout);

        for (auto& attrib : layout.attributes) {
            auto attrib_desc     = VkVertexInputAttributeDescription{};
            attrib_desc.binding  = vertex_layout.binding;
            attrib_desc.format   = vkenum(attrib.format);
            attrib_desc.location = attrib.shader_location;
            attrib_desc.offset   = static_cast<uint32_t>(attrib.offset);
            vertex_attribs.push_back(attrib_desc);
        }
    }

    // color blend attachments
    Vector<VkFormat>                            formats{};
    Vector<VkPipelineColorBlendAttachmentState> attachments;
    for (auto& attachment : desc.fragment.targets) {
        auto blend                = VkPipelineColorBlendAttachmentState{};
        blend.blendEnable         = attachment.blend_enable;
        blend.colorWriteMask      = vkenum(attachment.write_mask);
        blend.colorBlendOp        = vkenum(attachment.blend.color.operation);
        blend.alphaBlendOp        = vkenum(attachment.blend.alpha.operation);
        blend.srcColorBlendFactor = vkenum(attachment.blend.color.src_factor);
        blend.dstColorBlendFactor = vkenum(attachment.blend.color.dst_factor);
        blend.srcAlphaBlendFactor = vkenum(attachment.blend.alpha.src_factor);
        blend.dstAlphaBlendFactor = vkenum(attachment.blend.alpha.dst_factor);
        attachments.push_back(blend);
        formats.push_back(vkenum(attachment.format));
    }

    // use dummy swapchain extent for non-windowed workload,
    // but also query from the first available swapchain
    VkExtent2D extent = {1920, 1080};
    if (rhi->swapchains.range_check(0))
        extent = rhi->swapchains.at(0).extent;

    // dummy viewport (supposed to be replaced by vkCmdSetViewport)
    auto viewport     = VkViewport{};
    viewport.x        = 0;
    viewport.y        = 0;
    viewport.width    = static_cast<float>(extent.width);
    viewport.height   = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    // dummy scissor (supposed to be replaced by vkCmdSetScissor)
    auto scissor          = VkRect2D{};
    scissor.offset.x      = 0;
    scissor.offset.y      = 0;
    scissor.extent.width  = extent.width;
    scissor.extent.height = extent.height;

    // allow viewport/scissor/etc to be reset at rendering time
    Vector<VkDynamicState> dynamics = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_LINE_WIDTH,
        VK_DYNAMIC_STATE_BLEND_CONSTANTS,
        VK_DYNAMIC_STATE_STENCIL_REFERENCE,
    };

    auto vertex_input_state                            = VkPipelineVertexInputStateCreateInfo{};
    vertex_input_state.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_state.pNext                           = nullptr;
    vertex_input_state.pVertexBindingDescriptions      = vertex_layouts.data();
    vertex_input_state.vertexBindingDescriptionCount   = static_cast<uint>(vertex_layouts.size());
    vertex_input_state.pVertexAttributeDescriptions    = vertex_attribs.data();
    vertex_input_state.vertexAttributeDescriptionCount = static_cast<uint>(vertex_attribs.size());

    auto input_assembly_state                   = VkPipelineInputAssemblyStateCreateInfo{};
    input_assembly_state.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_state.pNext                  = nullptr;
    input_assembly_state.topology               = vkenum(desc.primitive.topology);
    input_assembly_state.primitiveRestartEnable = false;

    auto tessellation_state  = VkPipelineTessellationStateCreateInfo{};
    tessellation_state.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    tessellation_state.pNext = nullptr;

    auto viewport_state          = VkPipelineViewportStateCreateInfo{};
    viewport_state.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.pNext         = nullptr;
    viewport_state.pViewports    = &viewport;
    viewport_state.viewportCount = 1;
    viewport_state.pScissors     = &scissor;
    viewport_state.scissorCount  = 1;

    auto rasterization_state                    = VkPipelineRasterizationStateCreateInfo{};
    rasterization_state.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_state.pNext                   = nullptr;
    rasterization_state.cullMode                = vkenum(desc.primitive.cull_mode);
    rasterization_state.polygonMode             = VK_POLYGON_MODE_FILL;
    rasterization_state.frontFace               = vkenum(desc.primitive.front_face);
    rasterization_state.lineWidth               = 1;
    rasterization_state.rasterizerDiscardEnable = VK_FALSE;
    rasterization_state.depthBiasEnable         = desc.depth_stencil.depth_bias;
    rasterization_state.depthBiasConstantFactor = desc.depth_stencil.depth_bias_constant;
    rasterization_state.depthBiasSlopeFactor    = desc.depth_stencil.depth_bias_slope_scale;
    rasterization_state.depthBiasClamp          = desc.depth_stencil.depth_bias_clamp;

    auto multisample_state                  = VkPipelineMultisampleStateCreateInfo{};
    multisample_state.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_state.pNext                 = nullptr;
    multisample_state.pSampleMask           = nullptr;
    multisample_state.sampleShadingEnable   = VK_FALSE;
    multisample_state.minSampleShading      = 0;
    multisample_state.alphaToOneEnable      = desc.multisample.alpha_to_one_enabled;
    multisample_state.alphaToCoverageEnable = desc.multisample.alpha_to_coverage_enabled;
    multisample_state.rasterizationSamples  = vkenum(int(desc.multisample.count));

    auto depth_stencil_state                  = VkPipelineDepthStencilStateCreateInfo{};
    depth_stencil_state.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil_state.pNext                 = nullptr;
    depth_stencil_state.depthTestEnable       = desc.depth_stencil.depth_compare != GPUCompareFunction::ALWAYS || desc.depth_stencil.depth_write_enabled;
    depth_stencil_state.depthWriteEnable      = desc.depth_stencil.depth_write_enabled;
    depth_stencil_state.depthBoundsTestEnable = VK_FALSE;
    depth_stencil_state.depthCompareOp        = vkenum(desc.depth_stencil.depth_compare);
    depth_stencil_state.maxDepthBounds        = 1.0f;
    depth_stencil_state.minDepthBounds        = 0.0f;
    depth_stencil_state.stencilTestEnable =
        desc.depth_stencil.stencil_front.compare != GPUCompareFunction::ALWAYS ||
        desc.depth_stencil.stencil_front.pass_op != GPUStencilOperation::KEEP ||
        desc.depth_stencil.stencil_front.fail_op != GPUStencilOperation::KEEP ||
        desc.depth_stencil.stencil_back.compare != GPUCompareFunction::ALWAYS ||
        desc.depth_stencil.stencil_back.pass_op != GPUStencilOperation::KEEP ||
        desc.depth_stencil.stencil_back.fail_op != GPUStencilOperation::KEEP;
    depth_stencil_state.front.compareOp   = vkenum(desc.depth_stencil.stencil_front.compare);
    depth_stencil_state.front.depthFailOp = vkenum(desc.depth_stencil.stencil_front.depth_fail_op);
    depth_stencil_state.front.failOp      = vkenum(desc.depth_stencil.stencil_front.fail_op);
    depth_stencil_state.front.passOp      = vkenum(desc.depth_stencil.stencil_front.pass_op);
    depth_stencil_state.front.writeMask   = desc.depth_stencil.stencil_write_mask;
    depth_stencil_state.front.compareMask = desc.depth_stencil.stencil_read_mask;
    depth_stencil_state.front.reference   = 0; // need to be set dynamically
    depth_stencil_state.back.compareOp    = vkenum(desc.depth_stencil.stencil_back.compare);
    depth_stencil_state.back.depthFailOp  = vkenum(desc.depth_stencil.stencil_back.depth_fail_op);
    depth_stencil_state.back.failOp       = vkenum(desc.depth_stencil.stencil_back.fail_op);
    depth_stencil_state.back.passOp       = vkenum(desc.depth_stencil.stencil_back.pass_op);
    depth_stencil_state.back.writeMask    = desc.depth_stencil.stencil_write_mask;
    depth_stencil_state.back.compareMask  = desc.depth_stencil.stencil_read_mask;
    depth_stencil_state.back.reference    = 0; // need to be set dynamically

    auto color_blend_state            = VkPipelineColorBlendStateCreateInfo{};
    color_blend_state.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_state.pNext           = nullptr;
    color_blend_state.logicOpEnable   = VK_FALSE;
    color_blend_state.logicOp         = VK_LOGIC_OP_NO_OP;
    color_blend_state.pAttachments    = attachments.data();
    color_blend_state.attachmentCount = static_cast<uint>(attachments.size());

    auto dynamic_states              = VkPipelineDynamicStateCreateInfo{};
    dynamic_states.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_states.pNext             = nullptr;
    dynamic_states.pDynamicStates    = dynamics.data();
    dynamic_states.dynamicStateCount = static_cast<uint>(dynamics.size());

    auto pipeline_rendering_create_info                    = VkPipelineRenderingCreateInfo{};
    pipeline_rendering_create_info.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    pipeline_rendering_create_info.pNext                   = nullptr;
    pipeline_rendering_create_info.colorAttachmentCount    = static_cast<uint>(formats.size());
    pipeline_rendering_create_info.pColorAttachmentFormats = formats.data();
    pipeline_rendering_create_info.depthAttachmentFormat   = VK_FORMAT_UNDEFINED;
    pipeline_rendering_create_info.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
    if (depth_stencil_state.depthWriteEnable || depth_stencil_state.depthTestEnable || depth_stencil_state.stencilTestEnable) {
        if (is_depth_format(desc.depth_stencil.format))
            pipeline_rendering_create_info.depthAttachmentFormat = vkenum(desc.depth_stencil.format);
        if (is_stencil_format(desc.depth_stencil.format))
            pipeline_rendering_create_info.stencilAttachmentFormat = vkenum(desc.depth_stencil.format);
    }

    auto create_info                = VkGraphicsPipelineCreateInfo{};
    create_info.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    create_info.pNext               = &pipeline_rendering_create_info;
    create_info.layout              = layout.layout;
    create_info.flags               = VkPipelineCreateFlags(0);
    create_info.pStages             = stages.data();
    create_info.stageCount          = static_cast<uint>(stages.size());
    create_info.pVertexInputState   = &vertex_input_state;
    create_info.pInputAssemblyState = &input_assembly_state;
    create_info.pTessellationState  = &tessellation_state;
    create_info.pViewportState      = &viewport_state;
    create_info.pRasterizationState = &rasterization_state;
    create_info.pMultisampleState   = &multisample_state;
    create_info.pDepthStencilState  = &depth_stencil_state;
    create_info.pColorBlendState    = &color_blend_state;
    create_info.pDynamicState       = &dynamic_states;
    create_info.subpass             = 0;              // enable feature: dynamic rendering
    create_info.renderPass          = VK_NULL_HANDLE; // enable feature: dynamic rendering
                                                      //
    // TODO: support specialization constants

    this->layout = layout.layout; // record the pipeline layout
    vk_check(rhi->vtable.vkCreateGraphicsPipelines(rhi->device, cache, 1, &create_info, nullptr, &pipeline));

    if (desc.label)
        rhi->set_debug_label(VK_OBJECT_TYPE_PIPELINE, (uint64_t)pipeline, desc.label);
}

VulkanPipeline::VulkanPipeline(const GPUComputePipelineDescriptor& desc)
{
    auto rhi = get_rhi();

    auto& layout = fetch_resource(rhi->pipeline_layouts, desc.layout);
    auto& shader = fetch_resource(rhi->shaders, desc.compute.module);

    auto create_info   = VkComputePipelineCreateInfo{};
    create_info.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    create_info.pNext  = nullptr;
    create_info.layout = layout.layout;
    create_info.stage  = create_shader_stage(shader, desc.compute.entry_point, VK_SHADER_STAGE_COMPUTE_BIT);

    // TODO: support specialization constants

    this->layout = layout.layout; // record the pipeline layout
    vk_check(rhi->vtable.vkCreateComputePipelines(rhi->device, cache, 1, &create_info, nullptr, &pipeline));

    if (desc.label)
        rhi->set_debug_label(VK_OBJECT_TYPE_PIPELINE, (uint64_t)pipeline, desc.label);
}

VulkanPipeline::VulkanPipeline(const GPURayTracingPipelineDescriptor& desc)
{
    assert(!!!"Ray tracing pipeline is currently not supported!");
}

void VulkanPipeline::destroy()
{
    if (pipeline != VK_NULL_HANDLE) {
        auto rhi = get_rhi();
        rhi->vtable.vkDestroyPipeline(rhi->device, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }
}
