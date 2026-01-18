// reference: https://alain.xyz/blog/raw-directx12
#include "D3D12Utils.h"

D3D12Pipeline::D3D12Pipeline()
{
    pipeline = nullptr;
}

D3D12Pipeline::D3D12Pipeline(const GPUComputePipelineDescriptor& desc)
{
    pipeline = nullptr;

    auto rhi = get_rhi();

    D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};

    // root signature
    D3D12PipelineLayout& layout = fetch_resource(rhi->pipeline_layouts, desc.layout);
    pso_desc.pRootSignature     = layout.layout;

    // compute shader
    D3D12Shader& shader         = fetch_resource(rhi->shaders, desc.compute.module);
    pso_desc.CS.pShaderBytecode = shader.binary.data();
    pso_desc.CS.BytecodeLength  = static_cast<UINT>(shader.binary.size());

    this->layout = desc.layout;
    ThrowIfFailed(rhi->device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pipeline)));
}

D3D12Pipeline::D3D12Pipeline(const GPURenderPipelineDescriptor& desc)
{
    pipeline = nullptr;

    auto rhi = get_rhi();

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};

    // root signature
    D3D12PipelineLayout& layout = fetch_resource(rhi->pipeline_layouts, desc.layout);
    pso_desc.pRootSignature     = layout.layout;

    // vertex shader
    D3D12Shader& vs_shader      = fetch_resource(rhi->shaders, desc.vertex.module);
    pso_desc.VS.pShaderBytecode = vs_shader.binary.data();
    pso_desc.VS.BytecodeLength  = static_cast<UINT>(vs_shader.binary.size());

    // fragment shader
    D3D12Shader& fs_shader      = fetch_resource(rhi->shaders, desc.fragment.module);
    pso_desc.PS.pShaderBytecode = fs_shader.binary.data();
    pso_desc.PS.BytecodeLength  = static_cast<UINT>(fs_shader.binary.size());

    // vertex input layouts
    uint                             buffer_index = 0;
    Vector<D3D12_INPUT_ELEMENT_DESC> input_elements;
    input_elements.reserve(desc.vertex.buffers.size());
    vertex_buffer_strides.reserve(desc.vertex.buffers.size());
    for (auto& layout : desc.vertex.buffers) {

        for (const auto& attribute : layout.attributes) {
            D3D12_INPUT_ELEMENT_DESC element = {};
            element.SemanticName             = attribute.shader_semantic;
            element.SemanticIndex            = attribute.shader_location;
            element.Format                   = d3d12enum(attribute.format);
            element.InputSlot                = static_cast<UINT>(buffer_index);
            element.AlignedByteOffset        = static_cast<UINT>(attribute.offset);
            element.InputSlotClass           = d3d12enum(layout.step_mode);
            element.InstanceDataStepRate     = (layout.step_mode == GPUVertexStepMode::INSTANCE) ? 1 : 0;

            // record vertex input elements
            input_elements.push_back(element);
        }

        // record vertex buffer strides
        vertex_buffer_strides.push_back(static_cast<uint>(layout.array_stride));

        // increment buffer layout index
        buffer_index++;
    }

    // input assembly state
    pso_desc.InputLayout.pInputElementDescs = input_elements.data();
    pso_desc.InputLayout.NumElements        = static_cast<UINT>(input_elements.size());
    pso_desc.PrimitiveTopologyType          = infer_topology_type(desc.primitive.topology);

    // rasterizer state
    pso_desc.RasterizerState.FillMode              = D3D12_FILL_MODE_SOLID;
    pso_desc.RasterizerState.CullMode              = d3d12enum(desc.primitive.cull_mode);
    pso_desc.RasterizerState.FrontCounterClockwise = desc.primitive.front_face == GPUFrontFace::CCW;
    pso_desc.RasterizerState.DepthBias             = desc.depth_stencil.depth_bias;
    pso_desc.RasterizerState.DepthBiasClamp        = desc.depth_stencil.depth_bias_clamp;
    pso_desc.RasterizerState.SlopeScaledDepthBias  = desc.depth_stencil.depth_bias_slope_scale;
    pso_desc.RasterizerState.DepthClipEnable       = true;
    pso_desc.RasterizerState.MultisampleEnable     = desc.multisample.count > 1;
    pso_desc.RasterizerState.AntialiasedLineEnable = false;
    pso_desc.RasterizerState.ForcedSampleCount     = 0;
    pso_desc.RasterizerState.ConservativeRaster    = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    // depth stencil state
    pso_desc.DSVFormat                     = infer_texture_format(desc.depth_stencil.format);
    pso_desc.DepthStencilState.DepthEnable = desc.depth_stencil.depth_write_enabled || (desc.depth_stencil.depth_compare != GPUCompareFunction::ALWAYS);
    if (pso_desc.DepthStencilState.DepthEnable) {
        pso_desc.DepthStencilState.DepthWriteMask = desc.depth_stencil.depth_write_enabled ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
        pso_desc.DepthStencilState.DepthFunc      = d3d12enum(desc.depth_stencil.depth_compare, pso_desc.DepthStencilState.DepthEnable);
    }
    pso_desc.DepthStencilState.StencilEnable =
        desc.depth_stencil.stencil_front.compare != GPUCompareFunction::ALWAYS ||
        desc.depth_stencil.stencil_front.pass_op != GPUStencilOperation::KEEP ||
        desc.depth_stencil.stencil_front.fail_op != GPUStencilOperation::KEEP ||
        desc.depth_stencil.stencil_back.compare != GPUCompareFunction::ALWAYS ||
        desc.depth_stencil.stencil_back.pass_op != GPUStencilOperation::KEEP ||
        desc.depth_stencil.stencil_back.fail_op != GPUStencilOperation::KEEP;
    if (pso_desc.DepthStencilState.StencilEnable) {
        pso_desc.DepthStencilState.StencilReadMask              = desc.depth_stencil.stencil_read_mask;
        pso_desc.DepthStencilState.StencilWriteMask             = desc.depth_stencil.stencil_write_mask;
        pso_desc.DepthStencilState.FrontFace.StencilFunc        = d3d12enum(desc.depth_stencil.stencil_front.compare, true);
        pso_desc.DepthStencilState.FrontFace.StencilFailOp      = d3d12enum(desc.depth_stencil.stencil_front.fail_op);
        pso_desc.DepthStencilState.FrontFace.StencilPassOp      = d3d12enum(desc.depth_stencil.stencil_front.pass_op);
        pso_desc.DepthStencilState.FrontFace.StencilDepthFailOp = d3d12enum(desc.depth_stencil.stencil_front.depth_fail_op);
        pso_desc.DepthStencilState.BackFace.StencilFunc         = d3d12enum(desc.depth_stencil.stencil_back.compare, true);
        pso_desc.DepthStencilState.BackFace.StencilFailOp       = d3d12enum(desc.depth_stencil.stencil_back.fail_op);
        pso_desc.DepthStencilState.BackFace.StencilPassOp       = d3d12enum(desc.depth_stencil.stencil_back.pass_op);
        pso_desc.DepthStencilState.BackFace.StencilDepthFailOp  = d3d12enum(desc.depth_stencil.stencil_back.depth_fail_op);
    }

    // multisample state
    pso_desc.SampleDesc.Count   = desc.multisample.count;
    pso_desc.SampleDesc.Quality = 0;
    pso_desc.SampleMask         = desc.multisample.mask;

    // blend state
    pso_desc.BlendState.AlphaToCoverageEnable  = desc.multisample.alpha_to_coverage_enabled;
    pso_desc.BlendState.IndependentBlendEnable = FALSE; // Set to TRUE if different blend per RT

    // render target formats
    pso_desc.NumRenderTargets = static_cast<UINT>(desc.fragment.targets.size());
    for (size_t i = 0; i < desc.fragment.targets.size() && i < 8; ++i) {
        auto& attachment                                          = desc.fragment.targets[i];
        pso_desc.RTVFormats[i]                                    = infer_texture_format(attachment.format);
        pso_desc.BlendState.RenderTarget[i].RenderTargetWriteMask = attachment.write_mask.value;
        pso_desc.BlendState.RenderTarget[i].BlendEnable           = attachment.blend_enable;
        pso_desc.BlendState.RenderTarget[i].BlendOp               = d3d12enum(attachment.blend.color.operation);
        pso_desc.BlendState.RenderTarget[i].BlendOpAlpha          = d3d12enum(attachment.blend.alpha.operation);
        pso_desc.BlendState.RenderTarget[i].SrcBlend              = d3d12enum(attachment.blend.color.src_factor);
        pso_desc.BlendState.RenderTarget[i].SrcBlendAlpha         = d3d12enum(attachment.blend.alpha.src_factor);
        pso_desc.BlendState.RenderTarget[i].DestBlend             = d3d12enum(attachment.blend.color.dst_factor);
        pso_desc.BlendState.RenderTarget[i].DestBlendAlpha        = d3d12enum(attachment.blend.alpha.dst_factor);
        pso_desc.BlendState.RenderTarget[i].LogicOpEnable         = false;
    }

    this->layout   = desc.layout;
    this->topology = infer_topology(desc.primitive.topology);
    ThrowIfFailed(rhi->device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pipeline)));

    if (desc.label)
        pipeline->SetName(to_wstring(desc.label).c_str());
}

D3D12Pipeline::D3D12Pipeline(const GPURayTracingPipelineDescriptor& desc)
{
    get_logger()->error("D3D12 Ray Tracing Pipeline creation is not yet fully implemented due to missing shader information in GPURayTracingPipelineDescriptor.");
    assert(!!!"RayTracingPipeline is currently not supported!");
}

void D3D12Pipeline::destroy()
{
    if (pipeline) {
        pipeline->Release();
        pipeline = nullptr;
    }
}
