#include <Lyra/Utilities/Math.h>

#include "Renderer.h"
#include "Panels/SceneView.h"

static CString graphics_pipeline_program = R"""(
import lyra;

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

struct FragmentOutput
{
    float4 color : SV_Target;
    float  depth : SV_Depth;
};

struct Camera
{
    float4x4 proj;
    float4x4 view;
    float4x4 inv_proj;
    float4x4 inv_view;
    float3   pos;
    float    padding;
};

ConstantBuffer<Camera> camera;

[shader("vertex")]
VertexOutput vsmain(uint vertex_id : SV_VertexID)
{
    // fullscreen quad in NDC [-1, 1] using 6 vertices (2 triangles)
    static const float2 quad_vertices[6] = {
        float2(-1.0, -1.0),
        float2( 1.0, -1.0),
        float2( 1.0,  1.0),
        float2(-1.0, -1.0),
        float2( 1.0,  1.0),
        float2(-1.0,  1.0)
    };

    float2 p = quad_vertices[vertex_id];

    VertexOutput output;
    output.position = float4(p, 0.0, 1.0);
    output.uv       = p;
    return output;
}

float compute_grid(float2 coord, float scale, float line_width)
{
    float2 uv = coord / scale;
    float2 d = max(fwidth(uv), float2(1e-6, 1e-6));
    float2 g = abs(frac(uv - 0.5) - 0.5) / d;
    float line_dist = min(g.x, g.y);
    return 1.0 - min(line_dist / line_width, 1.0);
}

[shader("fragment")]
FragmentOutput fsmain(VertexOutput input)
{
    // unproject near and far points from NDC (-1 to 1) to view then world space
    float4 p_near_clip = float4(input.uv, 0.0, 1.0);
    float4 p_far_clip  = float4(input.uv, 1.0, 1.0);

    float4 p_near_view = mul(p_near_clip, camera.inv_proj);
    p_near_view /= p_near_view.w;
    float3 p_near_world = mul(float4(p_near_view.xyz, 1.0), camera.inv_view).xyz;

    float4 p_far_view = mul(p_far_clip, camera.inv_proj);
    p_far_view /= p_far_view.w;
    float3 p_far_world = mul(float4(p_far_view.xyz, 1.0), camera.inv_view).xyz;

    float3 ray_dir = p_far_world - p_near_world;
    if (abs(ray_dir.y) < 1e-6)
        discard;

    // intersection with ground plane Y = 0
    float t = -p_near_world.y / ray_dir.y;
    if (t <= 0.0)
        discard;

    float3 world_pos = p_near_world + t * ray_dir;

    // compute clip-space depth for depth buffer
    float4 clip_pos = mul(mul(float4(world_pos, 1.0), camera.view), camera.proj);
    float depth = clip_pos.z / clip_pos.w;

    if (depth < 0.0 || depth > 1.0)
        discard;

    // screen-space derivatives for anti-aliasing
    float2 d_coord = max(fwidth(world_pos.xz), float2(1e-6, 1e-6));

    // distance-based radial LOD calculation (uniform around the camera, avoiding elliptical artifacts)
    float dist = length(world_pos - camera.pos);
    float cam_alt = max(abs(camera.pos.y), 1.5);

    // 1-meter fine grid: fully visible nearby, smoothly dissolves over a broad distance range
    float fine_start = cam_alt * 6.0;
    float fine_end   = cam_alt * 18.0;
    float fine_lod   = 1.0 - smoothstep(fine_start, fine_end, dist);

    // 10-meter coarse grid: remains visible far out into the distance
    float coarse_start = cam_alt * 25.0;
    float coarse_end   = cam_alt * 65.0;
    float coarse_lod   = 1.0 - smoothstep(coarse_start, coarse_end, dist);

    // 0.1-meter sub-grid: softly appears only when zoomed in close to the ground (camera Y < 2.0)
    float sub_weight = smoothstep(2.0, 0.6, abs(camera.pos.y));
    float sub_lod    = (1.0 - smoothstep(2.0, 8.0, dist)) * sub_weight;

    // compute grid lines for active levels
    float line_01m = (sub_lod > 0.001)    ? compute_grid(world_pos.xz, 0.1,  0.8)  : 0.0;
    float line_1m  = (fine_lod > 0.001)   ? compute_grid(world_pos.xz, 1.0,  1.0)  : 0.0;
    float line_10m = (coarse_lod > 0.001) ? compute_grid(world_pos.xz, 10.0, 1.25) : 0.0;

    // opacities for each level
    float alpha_01m = line_01m * 0.15 * sub_lod;
    float alpha_1m  = line_1m  * 0.30 * fine_lod;
    float alpha_10m = line_10m * 0.60 * coarse_lod;

    // maximum blending: major 10m lines stay solid while intermediate 1m lines dissolve seamlessly
    float alpha = max(alpha_01m, max(alpha_1m, alpha_10m));

    // color grading from fine lines to major subdivision lines
    float3 col = float3(0.48, 0.48, 0.52);
    if (line_10m > 0.01 && coarse_lod > 0.01) {
        col = lerp(col, float3(0.72, 0.72, 0.76), line_10m * coarse_lod);
    }

    // coordinate axes: X axis (Z == 0) in Red, Z axis (X == 0) in Blue
    float x_axis_dist = abs(world_pos.z) / d_coord.y;
    float is_x_axis = 1.0 - min(x_axis_dist / 1.5, 1.0);

    float z_axis_dist = abs(world_pos.x) / d_coord.x;
    float is_z_axis = 1.0 - min(z_axis_dist / 1.5, 1.0);

    if (is_x_axis > 0.001) {
        col = lerp(col, float3(0.88, 0.25, 0.25), is_x_axis);
        alpha = max(alpha, is_x_axis * 0.90 * coarse_lod);
    }
    if (is_z_axis > 0.001) {
        col = lerp(col, float3(0.25, 0.50, 0.90), is_z_axis);
        alpha = max(alpha, is_z_axis * 0.90 * coarse_lod);
    }

    // soft grazing angle fade near the horizon to avoid edge shimmer
    float3 view_dir = normalize(world_pos - camera.pos);
    float angle_fade = smoothstep(0.005, 0.05, abs(view_dir.y));
    alpha *= angle_fade;

    if (alpha <= 0.002)
        discard;

    FragmentOutput output;
    output.color = float4(col, alpha);
    output.depth = depth;
    return output;
}
)""";

struct Camera
{
    glm::mat4 proj;
    glm::mat4 view;
    glm::mat4 inv_proj;
    glm::mat4 inv_view;
    glm::vec3 pos;
    float     padding;
};

void SampleCubeRenderer::bind(Application& app)
{
    app.get_toolboard().add<SampleCubeRenderer*>(this);

    app.bind<AppEvent::INIT, &SampleCubeRenderer::init>(*this);
    app.bind<AppEvent::UPDATE, &SampleCubeRenderer::update>(*this);
    app.bind<AppEvent::DESTROY, &SampleCubeRenderer::destroy>(*this);
}

void SampleCubeRenderer::render(const Backbuffer& backbuffer, AppContext& context, GPUCommandBuffer command)
{
    // color attachments
    auto color_attachment        = GPURenderPassColorAttachment{};
    color_attachment.clear_value = GPUColor{0.12f, 0.12f, 0.14f, 1.0f};
    color_attachment.load_op     = GPULoadOp::CLEAR;
    color_attachment.store_op    = GPUStoreOp::STORE;
    color_attachment.view        = backbuffer.texview;

    // depth attachments
    auto depth_attachment              = GPURenderPassDepthStencilAttachment{};
    depth_attachment.view              = depth_view;
    depth_attachment.depth_load_op     = GPULoadOp::CLEAR;
    depth_attachment.depth_store_op    = GPUStoreOp::STORE;
    depth_attachment.depth_clear_value = 1.0f;

    // render pass info
    auto render_pass                     = GPURenderPassDescriptor{};
    render_pass.color_attachments        = color_attachment;
    render_pass.depth_stencil_attachment = depth_attachment;

    command.push_debug_group("Renderer");
    command.resource_barrier(state_transition(backbuffer.texture, undefined_state(), color_attachment_state()));
    command.resource_barrier(state_transition(depth_texture, undefined_state(), depth_stencil_attachment_state()));
    command.begin_render_pass(render_pass);
    command.set_viewport(0, 0, static_cast<float>(backbuffer.extent.width), static_cast<float>(backbuffer.extent.height));
    command.set_scissor_rect(0, 0, backbuffer.extent.width, backbuffer.extent.height);
    command.set_pipeline(pipeline);
    command.set_bind_group(0, bind_group);

    // render infinite grid plane (fullscreen quad, procedural vertices)
    command.draw(6, 1, 0, 0);

    command.end_render_pass();
    command.resource_barrier(state_transition(backbuffer.texture, color_attachment_state(), shader_resource_state(GPUBarrierSync::ALL_SHADING)));
    command.pop_debug_group();
}

void SampleCubeRenderer::init(AppContext& context)
{
    auto device   = context.toolboard.get<GPUDevice*>();
    auto compiler = context.toolboard.get<Compiler*>();

    init_pipeline(*device, *compiler);
    init_buffers(*device);
    init_bind_group(*device);

    // initialize scene nodes
    if (auto* world = context.toolboard.try_get<World>()) {
        // create camera node looking down at the grid plane
        camera_node = world->create("Main Camera");
        world->translate(camera_node, {0.0f, 3.0f, 8.0f});
        world->rotate(camera_node, {1.0f, 0.0f, 0.0f}, -20.0f);
        world->add_component<PerspectiveCamera>(camera_node);
        world->add_component<CameraProjection>(camera_node);
    }
}

void SampleCubeRenderer::destroy(AppContext& context)
{
    auto device = context.toolboard.get<GPUDevice*>();
    device->wait();

    vshader.destroy();
    fshader.destroy();
    playout.destroy();
    pipeline.destroy();
    ubuffer.destroy();
    depth_texture.destroy();
    depth_view.destroy();
}

void SampleCubeRenderer::update(AppContext& context)
{
    auto* world     = context.toolboard.try_get<World>();
    auto* hierarchy = context.toolboard.try_get<SceneTree>();
    auto* scene     = context.toolboard.try_get<SceneView>();
    if (!world || !hierarchy || !scene) return;

    // update all transforms in the hierarchy
    hierarchy->update();

    // update camera uniform buffer
    auto backbuffer = scene->get_backbuffer();

    // ensure depth buffer matches backbuffer size
    if (!depth_texture.handle.valid() ||
        depth_texture.width != backbuffer.extent.width ||
        depth_texture.height != backbuffer.extent.height) {
        auto device = context.toolboard.get<GPUDevice*>();

        if (depth_texture.handle.valid()) {
            device->wait();
            depth_texture.destroy();
            depth_view.destroy();
        }

        depth_texture = execute([&]() {
            auto desc            = GPUTextureDescriptor{};
            desc.label           = "depth_buffer";
            desc.size.width      = backbuffer.extent.width;
            desc.size.height     = backbuffer.extent.height;
            desc.size.depth      = 1;
            desc.dimension       = GPUTextureDimension::x2D;
            desc.format          = GPUTextureFormat::DEPTH32FLOAT;
            desc.usage           = GPUTextureUsage::RENDER_ATTACHMENT;
            desc.mip_level_count = 1;
            desc.sample_count    = 1;
            return device->create_texture(desc);
        });
        depth_view    = depth_texture.create_view();
    }

    auto  aspect    = (float)backbuffer.extent.width / (float)backbuffer.extent.height;
    auto& cam_world = world->get_component<TransformWorld>(camera_node);

    // update camera projection parameters
    auto& cam_perspective  = world->get_component<PerspectiveCamera>(camera_node);
    cam_perspective.aspect = aspect;

    // get updated projection from RenderLayer (note: this might be 1 frame late if aspect ratio just changed)
    auto& cam_projection = world->get_component<CameraProjection>(camera_node);

    auto camera           = ubuffer.get_mapped_range<Camera>();
    camera.at(0).proj     = cam_projection.projection;
    camera.at(0).view     = glm::inverse(cam_world.xform);
    camera.at(0).inv_proj = glm::inverse(cam_projection.projection);
    camera.at(0).inv_view = cam_world.xform;
    camera.at(0).pos      = glm::vec3(cam_world.xform[3]);
    camera.at(0).padding  = 0.0f;
}

void SampleCubeRenderer::init_buffers(GPUDevice device)
{
    ubuffer = execute([&]() {
        auto desc               = GPUBufferDescriptor{};
        desc.label              = "camera_buffer";
        desc.size               = sizeof(Camera);
        desc.usage              = GPUBufferUsage::UNIFORM | GPUBufferUsage::MAP_WRITE;
        desc.mapped_at_creation = true;
        return device.create_buffer(desc);
    });
}

void SampleCubeRenderer::init_pipeline(GPUDevice device, Compiler compiler)
{
    auto module = execute([&]() {
        auto desc   = CompileDescriptor{};
        desc.module = "grid";
        desc.path   = "grid.slang";
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
        // color attachments with alpha blending enabled
        Vector<GPUColorTargetState> rstates;
        auto                        color_state = GPUColorTargetState{};
        color_state.format                      = GPUTextureFormat::RGBA8UNORM;
        color_state.blend_enable                = true;
        color_state.blend.color.operation       = GPUBlendOperation::ADD;
        color_state.blend.color.src_factor      = GPUBlendFactor::SRC_ALPHA;
        color_state.blend.color.dst_factor      = GPUBlendFactor::ONE_MINUS_SRC_ALPHA;
        color_state.blend.alpha.operation       = GPUBlendOperation::ADD;
        color_state.blend.alpha.src_factor      = GPUBlendFactor::ONE;
        color_state.blend.alpha.dst_factor      = GPUBlendFactor::ONE_MINUS_SRC_ALPHA;
        rstates.push_back(color_state);

        // render pipeline (no vertex buffers needed)
        auto desc                                  = GPURenderPipelineDescriptor{};
        desc.layout                                = playout;
        desc.primitive.cull_mode                   = GPUCullMode::NONE;
        desc.primitive.topology                    = GPUPrimitiveTopology::TRIANGLE_LIST;
        desc.primitive.front_face                  = GPUFrontFace::CCW;
        desc.primitive.strip_index_format          = GPUIndexFormat::UINT32;
        desc.depth_stencil.depth_compare           = GPUCompareFunction::LESS_EQUAL;
        desc.depth_stencil.depth_write_enabled     = false;
        desc.depth_stencil.format                  = GPUTextureFormat::DEPTH32FLOAT;
        desc.multisample.alpha_to_coverage_enabled = false;
        desc.multisample.count                     = 1;
        desc.vertex.module                         = vshader;
        desc.vertex.buffers                        = {};
        desc.vertex.entry_point                    = "vsmain";
        desc.fragment.module                       = fshader;
        desc.fragment.targets                      = rstates;
        desc.fragment.entry_point                  = "fsmain";
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
