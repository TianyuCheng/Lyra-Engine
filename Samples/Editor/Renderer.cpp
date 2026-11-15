#include "Renderer.h"
#include <Lyra/Utilities/Math.h>
#include <Lyra/Components/Camera.h>
#include <Lyra/Components/Transform.h>
#include <Lyra/Rendering/RendererManager.h>
#include <Lyra/Rendering/RenderWorld.h>

#include "Panels/SceneView.h"

using namespace lyra;

void EditorRenderer::bind(Application& app)
{
    app.get_services().add<EditorRenderer*>(this);

    app.bind<AppEvent::INIT, &EditorRenderer::init>(*this);
    app.bind<AppEvent::UPDATE, &EditorRenderer::update>(*this);
    app.bind<AppEvent::DESTROY, &EditorRenderer::destroy>(*this);
}

void EditorRenderer::init(AppContext& context)
{
    auto device   = context.services.get<GPUDevice*>();
    auto compiler = context.services.get<Compiler*>();

    // initialize composable editor grid pass
    grid_pass.init(*device, *compiler, GPUTextureFormat::RGBA8UNORM, GPUTextureFormat::DEPTH32FLOAT);

    // automatically load the forward renderer plugin if no active renderer
    if (auto rm = context.services.try_get<RendererManager*>()) {
        if (!rm->has_active_renderer()) {
            rm->load_renderer("lyra-renderer-forward", context);
        }
    }

    // initialize scene camera node if none exists
    if (auto world = context.services.try_get<World>()) {
        auto view = world->registry.view<Camera>();
        if (!view.empty()) {
            camera_node = SceneNode(*view.begin());
        } else {
            camera_node = world->create("Main Camera");
            world->translate(camera_node, {0.0f, 1.0f, 8.0f});
            world->rotate(camera_node, {1.0f, 0.0f, 0.0f}, -20.0f);
            world->add_component<Camera>(camera_node);
        }
    }
}

void EditorRenderer::destroy(AppContext& context)
{
    auto device = context.services.get<GPUDevice*>();
    device->wait();

    grid_pass.destroy();

    if (depth_texture.handle.valid()) {
        depth_texture.destroy();
    }
    if (depth_view.handle.valid()) {
        depth_view.destroy();
    }

    fg_allocator.clear();
}

void EditorRenderer::update(AppContext& context)
{
    auto world     = context.services.try_get<World>();
    auto hierarchy = context.services.try_get<SceneTree>();
    auto scene     = context.services.try_get<SceneView>();
    if (!world || !hierarchy || !scene) return;

    // update all transforms in the hierarchy
    hierarchy->update();

    // ensure camera_node is valid across scene loads and clears
    if (camera_node.entity == entt::null || !world->registry.valid(camera_node.entity) || !world->registry.any_of<Camera>(camera_node.entity)) {
        auto view = world->registry.view<Camera, TransformWorld>();
        if (view.begin() != view.end()) {
            camera_node = SceneNode(*view.begin());
        } else {
            camera_node = world->create("Main Camera");
            world->translate(camera_node, {0.0f, 1.0f, 8.0f});
            world->rotate(camera_node, {1.0f, 0.0f, 0.0f}, -20.0f);
            world->add_component<Camera>(camera_node);
        }
    }

    // update camera uniform buffer
    auto backbuffer = scene->get_backbuffer();
    if (backbuffer.extent.width == 0 || backbuffer.extent.height == 0) {
        return;
    }

    // ensure depth buffer matches backbuffer size
    if (!depth_texture.handle.valid() ||
        depth_texture.width != backbuffer.extent.width ||
        depth_texture.height != backbuffer.extent.height) {
        auto device = context.services.get<GPUDevice*>();

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
            desc.usage           = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;
            desc.mip_level_count = 1;
            desc.sample_count    = 1;
            return device->create_texture(desc);
        });
        depth_view    = depth_texture.create_view();
    }

    auto aspect = (float)backbuffer.extent.width / (float)backbuffer.extent.height;

    // update camera projection parameters
    auto& cam  = world->get_component<Camera>(camera_node);
    cam.aspect = aspect;
    if (cam.type == ProjectionType::PERSPECTIVE) {
        cam.projection = glm::perspective(glm::radians(cam.fov), cam.aspect, cam.near_plane, cam.far_plane);
    } else {
        float half_size = cam.size * 0.5f;
        cam.projection  = glm::ortho(-half_size * cam.aspect, half_size * cam.aspect, -half_size, half_size, cam.near_plane, cam.far_plane);
    }
}

void EditorRenderer::render(const Backbuffer& backbuffer, AppContext& context, GPUCommandBuffer command)
{
    if (!backbuffer.texture.valid() || !depth_texture.handle.valid() || backbuffer.extent.width == 0 || backbuffer.extent.height == 0) {
        return;
    }

    auto device = context.services.get<GPUDevice*>();
    auto world  = context.services.try_get<World>();
    if (!world || camera_node.entity == entt::null || !world->registry.valid(camera_node.entity)) {
        return;
    }

    auto& cam_comp  = world->get_component<Camera>(camera_node);
    auto& cam_world = world->get_component<TransformWorld>(camera_node);

    RenderView view{};
    view.projection      = cam_comp.projection;
    view.view            = glm::inverse(cam_world.xform);
    view.inv_projection  = glm::inverse(cam_comp.projection);
    view.inv_view        = cam_world.xform;
    view.view_projection = view.projection * view.view;
    view.camera_position = Vector3(cam_world.xform[3]);
    view.render_extent   = backbuffer.extent;
    view.near_plane      = cam_comp.near_plane;
    view.far_plane       = cam_comp.far_plane;
    view.fov             = cam_comp.fov;

    // extract scene representation from RenderWorld
    auto               render_world = context.services.try_get<RenderWorld*>();
    const RenderScene  dummy_scene{};
    const RenderScene& scene = render_world ? render_world->get_scene() : dummy_scene;

    // construct frame graph
    FrameGraphBuilder builder;

    // import backbuffer color target
    FrameGraphTexture imported_color{};
    imported_color.texture = backbuffer.texture;
    imported_color.view    = backbuffer.texview;
    imported_color.format  = backbuffer.format;
    imported_color.state   = undefined_texture_state();
    auto color_target      = builder.import(imported_color);

    // import scene depth buffer
    FrameGraphTexture imported_depth{};
    imported_depth.texture = depth_texture;
    imported_depth.view    = depth_view;
    imported_depth.format  = GPUTextureFormat::DEPTH32FLOAT;
    imported_depth.state   = undefined_texture_state();
    auto depth_target      = builder.import(imported_depth);

    // pass 1: scene clear pass (clears color and depth buffer to initial values)
    struct ClearPassData
    {
        FrameGraphHandle<FrameGraphTexture> color;
        FrameGraphHandle<FrameGraphTexture> depth;
    };

    auto clear_data = builder.add_pass<ClearPassData>(
        "SceneClearPass",
        [&](ClearPassData& data, FrameGraphPassBuilder& pass_builder) {
            data.color = pass_builder.render(color_target);
            data.depth = pass_builder.render(depth_target);
        },
        [extent = backbuffer.extent](const ClearPassData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
            auto color_tex = resources.get<FrameGraphTexture>(data.color);
            auto depth_tex = resources.get<FrameGraphTexture>(data.depth);
            if (!color_tex || !depth_tex || !ctx || !ctx->cmdlist.handle.valid()) {
                return;
            }

            auto color_attachment        = GPURenderPassColorAttachment{};
            color_attachment.view        = color_tex->view;
            color_attachment.clear_value = GPUColor{0.12f, 0.12f, 0.14f, 1.0f};
            color_attachment.load_op     = GPULoadOp::CLEAR;
            color_attachment.store_op    = GPUStoreOp::STORE;

            auto depth_attachment              = GPURenderPassDepthStencilAttachment{};
            depth_attachment.view              = depth_tex->view;
            depth_attachment.depth_clear_value = 1.0f;
            depth_attachment.depth_load_op     = GPULoadOp::CLEAR;
            depth_attachment.depth_store_op    = GPUStoreOp::STORE;

            auto render_pass                     = GPURenderPassDescriptor{};
            render_pass.color_attachments        = color_attachment;
            render_pass.depth_stencil_attachment = depth_attachment;

            ctx->cmdlist.begin_render_pass(render_pass);
            ctx->cmdlist.set_viewport(0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height));
            ctx->cmdlist.set_scissor_rect(0, 0, extent.width, extent.height);
            ctx->cmdlist.end_render_pass();
        });

    auto current_color = clear_data.color;
    auto current_depth = clear_data.depth;

    // pass 2: Forward Scene Rendering (evaluates skybox, meshes, lights, shadows)
    auto rm = context.services.try_get<RendererManager*>();
    if (rm && rm->has_active_renderer()) {
        rm->resize(backbuffer.extent.width, backbuffer.extent.height);

        RenderFrameOutputs outputs{};
        RenderFrameContext ctx{
            .builder        = builder,
            .view           = view,
            .scene          = scene,
            .color_target   = clear_data.color,
            .depth_target   = clear_data.depth,
            .color_format   = backbuffer.format,
            .depth_format   = GPUTextureFormat::DEPTH32FLOAT,
            .render_extent  = backbuffer.extent,
            .display_extent = backbuffer.extent,
            .outputs        = &outputs,
            .services       = &context.services,
        };

        current_color = rm->render(ctx);
        if (outputs.depth.valid()) {
            current_depth = outputs.depth;
        }
    }

    // pass 3: composable editor grid pass (depth-tests ground plane against rendered scene depth)
    auto grid_data = grid_pass.add(builder, current_color, current_depth, view);

    // pass 4: transition color target to shader resource so ImGui canvas can sample it
    builder.add_pass(
        "EditorViewportResolvePass",
        [&](FrameGraphPassBuilder& pass_builder) {
            (void)pass_builder.sample(grid_data.color_output);
            pass_builder.preserve();
        },
        [](FrameGraphResources&, FrameGraphContext*) {
            // transitions handled automatically by FrameGraph pre_read barrier
        });

    auto graph = builder.build();
    if (graph) {
        FrameGraphContext fg_context{};
        fg_context.device  = *device;
        fg_context.cmdlist = command;
        graph->execute(&fg_context, &fg_allocator);
    }
    fg_allocator.tick();
}
