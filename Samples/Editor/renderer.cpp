#include <Lyra/Common/Math.h>

#include "renderer.h"

static CString graphics_pipeline_program = R"""(
struct VertexInput
{
    float3 position : POSITION;
    float3 color    : COLOR0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float4 color    : COLOR0;
};

struct Camera
{
    float4x4 proj;
    float4x4 view;
};

ConstantBuffer<Camera> camera;

[shader("vertex")]
VertexOutput vsmain(VertexInput input)
{
    VertexOutput output;
    output.position = mul(mul(float4(input.position, 1.0), camera.view), camera.proj);
    output.color = float4(input.color, 1.0);
    return output;
}

[shader("fragment")]
float4 fsmain(VertexOutput input) : SV_Target
{
    return input.color;
}
)""";

struct Vertex
{
    glm::vec3 position;
    glm::vec3 color;
};

struct Camera
{
    glm::mat4 proj;
    glm::mat4 view;
};

void SampleCubeRenderer::bind(Application& app)
{
    app.get_blackboard().add<SampleCubeRenderer*>(this);

    app.bind<AppEvent::INIT>(&SampleCubeRenderer::init, this);
    app.bind<AppEvent::UPDATE>(&SampleCubeRenderer::update, this);
    app.bind<AppEvent::DESTROY>(&SampleCubeRenderer::destroy, this);
}

void SampleCubeRenderer::render(const Backbuffer& backbuffer, GPUDevice device, GPUCommandBuffer command)
{
    // color attachments
    auto color_attachment        = GPURenderPassColorAttachment{};
    color_attachment.clear_value = GPUColor{0.0f, 0.0f, 0.0f, 1.0f};
    color_attachment.load_op     = GPULoadOp::CLEAR;
    color_attachment.store_op    = GPUStoreOp::STORE;
    color_attachment.view        = backbuffer.texview;

    // render pass info
    auto render_pass                     = GPURenderPassDescriptor{};
    render_pass.color_attachments        = color_attachment;
    render_pass.depth_stencil_attachment = {};

    command.push_debug_group("Renderer");
    command.resource_barrier(state_transition(backbuffer.texture, undefined_state(), color_attachment_state()));
    command.begin_render_pass(render_pass);
    command.set_viewport(0, 0, static_cast<float>(backbuffer.extent.width), static_cast<float>(backbuffer.extent.height));
    command.set_scissor_rect(0, 0, backbuffer.extent.width, backbuffer.extent.height);
    command.set_pipeline(pipeline);
    command.set_vertex_buffer(0, vbuffer);
    command.set_index_buffer(ibuffer, GPUIndexFormat::UINT32);
    command.set_bind_group(0, bind_group);
    command.draw_indexed(24, 1, 0, 0, 0);
    command.end_render_pass();
    command.resource_barrier(state_transition(backbuffer.texture, color_attachment_state(), shader_resource_state(GPUBarrierSync::ALL_SHADING)));
    command.pop_debug_group();
}

void SampleCubeRenderer::init(Blackboard& blackboard)
{
    GPUDevice device   = blackboard.get<GPUDevice>();
    Compiler  compiler = blackboard.get<Compiler>();

    init_pipeline(device, compiler);
    init_buffers(device);
    init_bind_group(device);
}

void SampleCubeRenderer::destroy(Blackboard& blackboard)
{
    GPUDevice device = blackboard.get<GPUDevice>();
    device.wait();

    vshader.destroy();
    fshader.destroy();
    playout.destroy();
    pipeline.destroy();
    vbuffer.destroy();
    ibuffer.destroy();
    ubuffer.destroy();
}

void SampleCubeRenderer::update(Blackboard& blackboard)
{
    if (auto scene = blackboard.try_get<SceneView*>()) {
        auto backbuffer = (*scene)->get_backbuffer();
        auto aspect     = (float)backbuffer.extent.width / (float)backbuffer.extent.height;

        auto camera       = ubuffer.get_mapped_range<Camera>();
        camera.at(0).proj = glm::perspective(1.05f, aspect, 0.01f, 100.0f);
        camera.at(0).view = glm::lookAt(
            glm::vec3(0.0f, 0.0f, 3.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f));
    }
}

void SampleCubeRenderer::init_buffers(GPUDevice device)
{
    Vector<Vertex> vertices = {};
    Vector<uint>   indices  = {};
    uint           base     = 0;

    // clang-format off

    // front face
    base = static_cast<uint>(vertices.size());
    vertices.push_back({glm::vec3(-1.0f, -1.0f, 1.0f), glm::vec3(1.0f, 0.0f, 0.0f)});
    vertices.push_back({glm::vec3(+1.0f, -1.0f, 1.0f), glm::vec3(1.0f, 0.0f, 0.0f)});
    vertices.push_back({glm::vec3(+1.0f, +1.0f, 1.0f), glm::vec3(1.0f, 0.0f, 0.0f)});
    vertices.push_back({glm::vec3(-1.0f, +1.0f, 1.0f), glm::vec3(1.0f, 0.0f, 0.0f)});
    indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
    indices.push_back(base + 2); indices.push_back(base + 3); indices.push_back(base + 0);

    // back face
    base = static_cast<uint>(vertices.size());
    vertices.push_back({glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f)});
    vertices.push_back({glm::vec3(+1.0f, -1.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f)});
    vertices.push_back({glm::vec3(+1.0f, +1.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f)});
    vertices.push_back({glm::vec3(-1.0f, +1.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f)});
    indices.push_back(base + 0); indices.push_back(base + 3); indices.push_back(base + 2);
    indices.push_back(base + 2); indices.push_back(base + 1); indices.push_back(base + 0);

    // left face
    base = static_cast<uint>(vertices.size());
    vertices.push_back({glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(0.0f, 0.0f, 1.0f)});
    vertices.push_back({glm::vec3(-1.0f, +1.0f, -1.0f), glm::vec3(0.0f, 0.0f, 1.0f)});
    vertices.push_back({glm::vec3(-1.0f, +1.0f, +1.0f), glm::vec3(0.0f, 0.0f, 1.0f)});
    vertices.push_back({glm::vec3(-1.0f, -1.0f, +1.0f), glm::vec3(0.0f, 0.0f, 1.0f)});
    indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
    indices.push_back(base + 2); indices.push_back(base + 3); indices.push_back(base + 0);

    // right face
    base = static_cast<uint>(vertices.size());
    vertices.push_back({glm::vec3(+1.0f, -1.0f, -1.0f), glm::vec3(1.0f, 1.0f, 0.0f)});
    vertices.push_back({glm::vec3(+1.0f, +1.0f, -1.0f), glm::vec3(1.0f, 1.0f, 0.0f)});
    vertices.push_back({glm::vec3(+1.0f, +1.0f, +1.0f), glm::vec3(1.0f, 1.0f, 0.0f)});
    vertices.push_back({glm::vec3(+1.0f, -1.0f, +1.0f), glm::vec3(1.0f, 1.0f, 0.0f)});
    indices.push_back(base + 0); indices.push_back(base + 3); indices.push_back(base + 2);
    indices.push_back(base + 2); indices.push_back(base + 1); indices.push_back(base + 0);

    // top face
    base = static_cast<uint>(vertices.size());
    vertices.push_back({glm::vec3(-1.0f, +1.0f, -1.0f), glm::vec3(0.0f, 1.0f, 1.0f)});
    vertices.push_back({glm::vec3(+1.0f, +1.0f, -1.0f), glm::vec3(0.0f, 1.0f, 1.0f)});
    vertices.push_back({glm::vec3(+1.0f, +1.0f, +1.0f), glm::vec3(0.0f, 1.0f, 1.0f)});
    vertices.push_back({glm::vec3(-1.0f, +1.0f, +1.0f), glm::vec3(0.0f, 1.0f, 1.0f)});
    indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
    indices.push_back(base + 2); indices.push_back(base + 3); indices.push_back(base + 0);

    // bottom face
    base = static_cast<uint>(vertices.size());
    vertices.push_back({glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(1.0f, 0.0f, 1.0f)});
    vertices.push_back({glm::vec3(+1.0f, -1.0f, -1.0f), glm::vec3(1.0f, 0.0f, 1.0f)});
    vertices.push_back({glm::vec3(+1.0f, -1.0f, +1.0f), glm::vec3(1.0f, 0.0f, 1.0f)});
    vertices.push_back({glm::vec3(-1.0f, -1.0f, +1.0f), glm::vec3(1.0f, 0.0f, 1.0f)});
    indices.push_back(base + 0); indices.push_back(base + 3); indices.push_back(base + 2);
    indices.push_back(base + 2); indices.push_back(base + 1); indices.push_back(base + 0);

    // clang-format on

    vbuffer = execute([&]() {
        auto desc               = GPUBufferDescriptor{};
        desc.label              = "vertex_buffer";
        desc.size               = sizeof(Vertex) * vertices.size();
        desc.usage              = GPUBufferUsage::VERTEX | GPUBufferUsage::MAP_WRITE;
        desc.mapped_at_creation = true;
        return device.create_buffer(desc);
    });

    ibuffer = execute([&]() {
        auto desc               = GPUBufferDescriptor{};
        desc.label              = "index_buffer";
        desc.size               = sizeof(uint32_t) * indices.size();
        desc.usage              = GPUBufferUsage::INDEX | GPUBufferUsage::MAP_WRITE;
        desc.mapped_at_creation = true;
        return device.create_buffer(desc);
    });

    ubuffer = execute([&]() {
        auto desc               = GPUBufferDescriptor{};
        desc.label              = "camera_buffer";
        desc.size               = sizeof(Camera);
        desc.usage              = GPUBufferUsage::UNIFORM | GPUBufferUsage::MAP_WRITE;
        desc.mapped_at_creation = true;
        return device.create_buffer(desc);
    });

    auto p_vertices = vbuffer.get_mapped_range<Vertex>();
    for (uint i = 0; i < vertices.size(); i++)
        p_vertices.at(i) = vertices.at(i);

    auto p_indices = ibuffer.get_mapped_range<uint>();
    for (uint i = 0; i < indices.size(); i++)
        p_indices.at(i) = indices.at(i);
}

void SampleCubeRenderer::init_pipeline(GPUDevice device, Compiler compiler)
{
    auto module = execute([&]() {
        auto desc   = CompileDescriptor{};
        desc.module = "test";
        desc.path   = "test.slang";
        desc.source = graphics_pipeline_program;
        return compiler.compile(desc);
    });

    auto reflection = compiler.reflect({
        {*module, "vsmain"},
        {*module, "fsmain"},
    });

    vshader = lyra::execute([&]() {
        auto code  = module->get_shader_blob("vsmain");
        auto desc  = GPUShaderModuleDescriptor{};
        desc.label = "vertex_shader";
        desc.data  = code->data;
        desc.size  = code->size;
        return device.create_shader_module(desc);
    });

    fshader = lyra::execute([&]() {
        auto code  = module->get_shader_blob("fsmain");
        auto desc  = GPUShaderModuleDescriptor{};
        desc.label = "fragment_shader";
        desc.data  = code->data;
        desc.size  = code->size;
        return device.create_shader_module(desc);
    });

    playout = lyra::execute([&]() {
        blayouts.clear();
        for (auto& desc : reflection->get_bind_group_layouts()) {
            auto blayout = device.create_bind_group_layout(desc);
            blayouts.push_back(blayout);
        }
        auto desc                 = GPUPipelineLayoutDescriptor{};
        desc.bind_group_layouts   = blayouts;
        desc.push_constant_ranges = reflection->get_push_constant_ranges();
        return device.create_pipeline_layout(desc);
    });

    pipeline = lyra::execute([&]() {
        // vertex attributes
        Vector<ShaderAttribute> attributes;
        attributes.push_back({"position", offsetof(Vertex, position)});
        attributes.push_back({"color", offsetof(Vertex, color)});

        // vertex buffer layout
        auto attribs        = reflection->get_vertex_attributes(attributes);
        auto layout         = GPUVertexBufferLayout{};
        layout.attributes   = attribs;
        layout.array_stride = sizeof(Vertex);
        layout.step_mode    = GPUVertexStepMode::VERTEX;

        // color attachments
        Vector<GPUColorTargetState> rstates;
        auto                        color_state = GPUColorTargetState{};
        color_state.format                      = GPUTextureFormat::RGBA8UNORM;
        color_state.blend_enable                = false;
        rstates.push_back(color_state);

        // render pipeline
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
        desc.fragment.module                       = fshader;
        desc.vertex.buffers                        = layout;
        desc.fragment.targets                      = rstates;
        return device.create_render_pipeline(desc);
    });
}

void SampleCubeRenderer::init_bind_group(GPUDevice device)
{
    heap = lyra::execute([&]() {
        auto desc      = GPUBindGroupHeapDescriptor{};
        desc.page_size = 2048;
        return device.create_bind_group_heap(desc);
    });

    // create bind group
    bind_group = execute([&]() {
        Array<GPUBindGroupEntry, 1> entries = {};

        auto& entry         = entries.at(0);
        entry.type          = GPUResourceType::BUFFER;
        entry.binding       = 0;
        entry.buffer.buffer = ubuffer;
        entry.buffer.offset = 0;
        entry.buffer.size   = 0;

        auto desc    = GPUBindGroupDescriptor{};
        desc.heap    = heap;
        desc.layout  = blayouts.at(0);
        desc.entries = entries;
        return device.create_bind_group(desc);
    });
}
