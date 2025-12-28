#include "pipeline.h"

void SimplePipeline::init_color_state(GPUTextureFormat format, bool color_write)
{
    auto color_state         = GPUColorTargetState{};
    color_state.format       = format;
    color_state.blend_enable = false;
    if (!color_write)
        color_state.write_mask = GPUColorWrite::NONE;
    rstates.push_back(color_state);
}

void SimplePipeline::init_depth_state(GPUTextureFormat format)
{
    if (!dsstate.has_value()) dsstate = GPUDepthStencilState{};

    auto& state               = dsstate.value();
    state.format              = format;
    state.depth_compare       = GPUCompareFunction::LESS;
    state.depth_write_enabled = true;
}

void SimplePipeline::init_stencil_state(GPUTextureFormat format)
{
    if (!dsstate.has_value()) dsstate = GPUDepthStencilState{};

    auto& state                       = dsstate.value();
    state.format                      = format;
    state.depth_compare               = GPUCompareFunction::ALWAYS;
    state.depth_write_enabled         = false;
    state.stencil_read_mask           = 0x0;
    state.stencil_write_mask          = 0x1;
    state.stencil_front.depth_fail_op = GPUStencilOperation::KEEP;
    state.stencil_front.compare       = GPUCompareFunction::ALWAYS;
    state.stencil_front.pass_op       = GPUStencilOperation::REPLACE;
    state.stencil_front.fail_op       = GPUStencilOperation::REPLACE;
    state.stencil_back                = state.stencil_front;
}

void SimplePipeline::init_depth_stencil_state(GPUTextureFormat format)
{
    if (!dsstate.has_value()) dsstate = GPUDepthStencilState{};

    auto& state                       = dsstate.value();
    state.format                      = GPUTextureFormat::DEPTH24PLUS_STENCIL8;
    state.depth_compare               = GPUCompareFunction::ALWAYS;
    state.depth_write_enabled         = true;
    state.stencil_read_mask           = 0x1;
    state.stencil_write_mask          = 0x0;
    state.stencil_front.depth_fail_op = GPUStencilOperation::KEEP;
    state.stencil_front.compare       = GPUCompareFunction::EQUAL;
    state.stencil_front.pass_op       = GPUStencilOperation::KEEP; // don't modify stencil
    state.stencil_front.fail_op       = GPUStencilOperation::KEEP; // don't modify stencil
    state.stencil_back                = state.stencil_front;
}

void SimplePipeline::init_vshader(GPUDevice& device, ShaderModule* module, CString entry)
{
    auto code     = module->get_shader_blob(entry);
    auto desc     = GPUShaderModuleDescriptor{};
    desc.label    = "vertex_shader";
    desc.data     = code->data;
    desc.size     = code->size;
    vshader       = device.create_shader_module(desc);
    vshader_entry = entry;
}

void SimplePipeline::init_fshader(GPUDevice& device, ShaderModule* module, CString entry)
{
    auto code     = module->get_shader_blob(entry);
    auto desc     = GPUShaderModuleDescriptor{};
    desc.label    = "fragment_shader";
    desc.data     = code->data;
    desc.size     = code->size;
    fshader       = device.create_shader_module(desc);
    fshader_entry = entry;
}

void SimplePipeline::init_cshader(GPUDevice& device, ShaderModule* module, CString entry)
{
    auto code     = module->get_shader_blob(entry);
    auto desc     = GPUShaderModuleDescriptor{};
    desc.label    = "compute_shader";
    desc.data     = code->data;
    desc.size     = code->size;
    cshader       = device.create_shader_module(desc);
    cshader_entry = entry;
}

void SimplePipeline::init_playout(GPUDevice& device, ShaderReflection* reflection)
{
    for (auto& desc : reflection->get_bind_group_layouts()) {
        auto blayout = device.create_bind_group_layout(desc);
        blayouts.push_back(blayout);
    }

    auto desc                 = GPUPipelineLayoutDescriptor{};
    desc.bind_group_layouts   = blayouts;
    desc.push_constant_ranges = reflection->get_push_constant_ranges();
    playout                   = device.create_pipeline_layout(desc);
}

void SimpleComputePipeline::init_pipeline(GPUDevice& device, ShaderReflection* reflection)
{
    auto desc           = GPUComputePipelineDescriptor{};
    desc.layout         = playout;
    desc.compute.module = cshader;

    pipeline = device.create_compute_pipeline(desc);
}

void SimpleRenderPipeline::init_pipeline(GPUDevice& device, ShaderReflection* reflection)
{
    // vertex buffer layout
    auto attribs        = reflection->get_vertex_attributes(this->attributes);
    auto layout         = GPUVertexBufferLayout{};
    layout.attributes   = attribs;
    layout.array_stride = vstride;
    layout.step_mode    = GPUVertexStepMode::VERTEX;

    auto desc                                  = GPURenderPipelineDescriptor{};
    desc.layout                                = playout;
    desc.primitive.cull_mode                   = GPUCullMode::NONE;
    desc.primitive.topology                    = GPUPrimitiveTopology::TRIANGLE_LIST;
    desc.primitive.front_face                  = GPUFrontFace::CCW;
    desc.primitive.strip_index_format          = GPUIndexFormat::UINT32;
    desc.depth_stencil.depth_compare           = GPUCompareFunction::ALWAYS;
    desc.depth_stencil.depth_write_enabled     = false;
    desc.multisample.alpha_to_coverage_enabled = false;
    desc.multisample.count                     = 1;
    desc.vertex.module                         = vshader;
    desc.vertex.entry_point                    = vshader_entry;
    desc.fragment.module                       = fshader;
    desc.fragment.entry_point                  = fshader_entry;
    desc.vertex.buffers                        = layout;
    desc.fragment.targets                      = rstates;
    if (dsstate.has_value()) desc.depth_stencil = dsstate.value();

    pipeline = device.create_render_pipeline(desc);
}
