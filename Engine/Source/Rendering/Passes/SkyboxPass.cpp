#include <Lyra/Utilities/Macros.h>
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Rendering/Passes/SkyboxPass.h>

using namespace lyra;
using namespace lyra::passes;

namespace
{
    struct SkyboxUniform
    {
        Matrix4x4 proj;
        Matrix4x4 view;
        Matrix4x4 inv_proj;
        Matrix4x4 inv_view;
        Vector3   camera_pos;
        float     sun_intensity      = 110000.0f;
        Vector3   sun_dir            = Vector3(0.3f, 0.6f, 0.4f);
        float     sun_angular_radius = 0.004675f;
        Vector3   sun_color          = Vector3(1.0f);
        float     exposure_scale     = 1.0f / (1.2f * 8192.0f);
        float     halo_size          = 10.0f;
        float     halo_falloff       = 80.0f;
        float     limb_darkening     = 0.6f;
        float     ground_roughness   = 0.8f;
        Vector4   ground_albedo      = Vector4(0.12f, 0.12f, 0.14f, 1.0f);
    };
} // namespace

bool SkyboxPass::init(
    GPUDevice        device,
    Compiler         compiler,
    GPUTextureFormat color_format,
    GPUTextureFormat depth_format)
{
    if (initialized) {
        if (cached_color_format == color_format && cached_depth_format == depth_format) {
            return true;
        }
        destroy();
    }

    this->device              = device;
    this->cached_color_format = color_format;
    this->cached_depth_format = depth_format;

    // compile skybox shader asset
    auto module = execute([&]() {
        auto desc   = CompileDescriptor{};
        desc.module = "skybox";
        desc.path   = "sys://Shaders/Sky/Skybox.slang";
        desc.source = nullptr;
        return compiler.compile(desc);
    });

    if (!module) {
        return false;
    }

    auto reflection = compiler.reflect({
        {*module, "vsmain"},
        {*module, "fsmain"},
    });

    if (!reflection) {
        return false;
    }

    vshader = execute([&]() {
        auto code  = module->get_shader_blob("vsmain");
        auto desc  = GPUShaderModuleDescriptor{};
        desc.label = "skybox_vertex_shader";
        desc.data  = code->data;
        desc.size  = code->size;
        return device.create_shader_module(desc);
    });

    fshader = execute([&]() {
        auto code  = module->get_shader_blob("fsmain");
        auto desc  = GPUShaderModuleDescriptor{};
        desc.label = "skybox_fragment_shader";
        desc.data  = code->data;
        desc.size  = code->size;
        return device.create_shader_module(desc);
    });

    blayouts.clear();
    for (auto& desc : reflection->get_bind_group_layouts()) {
        auto blayout = device.create_bind_group_layout(desc);
        blayouts.push_back(blayout);
    }

    playout = execute([&]() {
        auto desc               = GPUPipelineLayoutDescriptor{};
        desc.bind_group_layouts = blayouts;
        desc.immediate_size     = reflection->get_immediate_size();
        return device.create_pipeline_layout(desc);
    });

    pipeline = execute([&]() {
        Vector<GPUColorTargetState> rstates;
        auto                        color_state = GPUColorTargetState{};
        color_state.format                      = cached_color_format;
        color_state.blend_enable                = false;
        rstates.push_back(color_state);

        auto desc                                  = GPURenderPipelineDescriptor{};
        desc.layout                                = playout;
        desc.primitive.cull_mode                   = GPUCullMode::NONE;
        desc.primitive.topology                    = GPUPrimitiveTopology::TRIANGLE_LIST;
        desc.primitive.front_face                  = GPUFrontFace::CCW;
        desc.primitive.strip_index_format          = GPUIndexFormat::UINT32;
        desc.depth_stencil.depth_compare           = GPUCompareFunction::LESS_EQUAL;
        desc.depth_stencil.depth_write_enabled     = false;
        desc.depth_stencil.format                  = cached_depth_format;
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

    for (uint i = 0; i < FRAMES_IN_FLIGHT; ++i) {
        camera_buffers[i] = execute([&]() {
            auto desc               = GPUBufferDescriptor{};
            desc.label              = "skybox_camera_buffer";
            desc.size               = sizeof(SkyboxUniform);
            desc.usage              = GPUBufferUsage::UNIFORM | GPUBufferUsage::MAP_WRITE;
            desc.mapped_at_creation = true;
            return device.create_buffer(desc);
        });
    }

    heap = execute([&]() {
        auto desc      = GPUBindGroupHeapDescriptor{};
        desc.page_size = 2048;
        return device.create_bind_group_heap(desc);
    });

    if (!blayouts.empty()) {
        for (uint i = 0; i < FRAMES_IN_FLIGHT; ++i) {
            bind_groups[i] = execute([&]() {
                Array<GPUBindGroupEntry, 1> entries = {};

                auto& entry         = entries.at(0);
                entry.type          = GPUResourceType::BUFFER;
                entry.binding       = 0;
                entry.buffer.buffer = camera_buffers[i];
                entry.buffer.offset = 0;
                entry.buffer.size   = 0;

                auto desc    = GPUBindGroupDescriptor{};
                desc.heap    = heap;
                desc.layout  = blayouts.at(0);
                desc.entries = entries;
                return device.create_bind_group(desc);
            });
        }
    }

    initialized = true;
    return true;
}

void SkyboxPass::destroy()
{
    if (!initialized) {
        return;
    }

    device.wait();

    if (heap.valid()) heap.destroy();
    for (auto& buf : camera_buffers) {
        if (buf.valid()) {
            buf.destroy();
        }
    }
    camera_buffers.fill({});
    bind_groups.fill({});

    if (pipeline.valid()) pipeline.destroy();
    if (playout.valid()) playout.destroy();
    if (fshader.valid()) fshader.destroy();
    if (vshader.valid()) vshader.destroy();
    blayouts.clear();

    initialized = false;
}

void SkyboxPass::update_camera_uniform(uint slot, const RenderView& view, const RenderScene& scene)
{
    if (slot >= FRAMES_IN_FLIGHT || !camera_buffers[slot].valid()) {
        return;
    }

    auto u = camera_buffers[slot].get_mapped_range<SkyboxUniform>();
    if (u.count == 0) {
        return;
    }

    u.at(0).proj           = view.projection;
    u.at(0).view           = view.view;
    u.at(0).inv_proj       = view.inv_projection;
    u.at(0).inv_view       = view.inv_view;
    u.at(0).camera_pos     = view.camera_position;
    u.at(0).exposure_scale = (view.exposure_scale > 0.0f) ? view.exposure_scale : (1.0f / (1.2f * 8192.0f));

    Vector3 sun_dir            = glm::normalize(Vector3(0.3f, 0.6f, 0.4f));
    Vector3 sun_color          = Vector3(1.0f);
    float   sun_intensity      = 110000.0f;
    float   sun_angular_radius = 0.004675f;
    float   halo_size          = 10.0f;
    float   halo_falloff       = 80.0f;
    float   limb_darkening     = 0.6f;

    if (!scene.directional_lights.empty()) {
        const auto& light = scene.directional_lights[0];
        // light.direction is propagation direction (pointing down towards ground).
        // sun direction in the sky is the opposite vector pointing towards the sun.
        if (glm::length2(light.direction) > 1e-4f) {
            sun_dir = -glm::normalize(light.direction);
        }
        sun_color          = light.color;
        sun_intensity      = light.intensity;
        sun_angular_radius = light.angular_radius;
        halo_size          = light.halo_size;
        halo_falloff       = light.halo_falloff;
        limb_darkening     = light.limb_darkening;
    }

    u.at(0).sun_intensity      = sun_intensity;
    u.at(0).sun_dir            = sun_dir;
    u.at(0).sun_angular_radius = sun_angular_radius;
    u.at(0).sun_color          = sun_color;
    u.at(0).halo_size          = halo_size;
    u.at(0).halo_falloff       = halo_falloff;
    u.at(0).limb_darkening     = limb_darkening;
    u.at(0).ground_roughness   = 0.8f;
    u.at(0).ground_albedo      = Vector4(0.12f, 0.12f, 0.14f, 1.0f);
}

SkyboxData SkyboxPass::add(
    FrameGraphBuilder&                  builder,
    FrameGraphHandle<FrameGraphTexture> color_target,
    FrameGraphHandle<FrameGraphTexture> depth_target,
    const RenderView&                   view,
    const RenderScene&                  scene,
    GPULoadOp                           depth_load_op,
    GPULoadOp                           color_load_op)
{
    frame_slot        = (frame_slot + 1) % FRAMES_IN_FLIGHT;
    uint current_slot = frame_slot;

    update_camera_uniform(current_slot, view, scene);

    return builder.add_pass<SkyboxData>(
        "SkyboxPass",
        [&](SkyboxData& data, FrameGraphPassBuilder& pass_builder) {
            data.color_output = pass_builder.render(color_target);
            data.depth_input  = pass_builder.read(depth_target);
        },
        [this, current_slot, depth_load_op, color_load_op, clear_color = scene.clear_color, extent = view.render_extent](const SkyboxData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
            if (!ctx || !ctx->cmdlist.handle.valid() || !pipeline.valid()) {
                return;
            }

            auto color_tex = resources.get<FrameGraphTexture>(data.color_output);
            auto depth_tex = resources.get<FrameGraphTexture>(data.depth_input);
            if (!color_tex || !depth_tex) {
                return;
            }

            auto color_attachment        = GPURenderPassColorAttachment{};
            color_attachment.view        = color_tex->view;
            color_attachment.clear_value = GPUColor{clear_color.r, clear_color.g, clear_color.b, clear_color.a};
            color_attachment.load_op     = color_load_op;
            color_attachment.store_op    = GPUStoreOp::STORE;

            auto depth_attachment              = GPURenderPassDepthStencilAttachment{};
            depth_attachment.view              = depth_tex->view;
            depth_attachment.depth_clear_value = 1.0f;
            depth_attachment.depth_load_op     = depth_load_op;
            depth_attachment.depth_store_op    = GPUStoreOp::STORE;

            auto desc                     = GPURenderPassDescriptor{};
            desc.color_attachments        = color_attachment;
            desc.depth_stencil_attachment = depth_attachment;

            auto command = ctx->cmdlist;
            command.begin_render_pass(desc);
            command.set_viewport(0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height));
            command.set_scissor_rect(0, 0, extent.width, extent.height);
            command.set_pipeline(pipeline);
            if (bind_groups[current_slot].valid()) {
                command.set_bind_group(0, bind_groups[current_slot]);
            }
            command.draw(6, 1, 0, 0);
            command.end_render_pass();
        });
}
