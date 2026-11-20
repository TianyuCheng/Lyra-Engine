#include "Renderer.h"
#include <Lyra/Utilities/Math.h>
#include <Lyra/Components/Light.h>
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
            rm->load_renderer(context, RendererBackend::FORWARD);
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
            camera_node = SceneNode{};
        }
    }

    if (camera_node.entity == entt::null || !world->registry.valid(camera_node.entity) || !world->registry.any_of<Camera>(camera_node.entity)) {
        return;
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

void EditorRenderer::render(const GPUSurfaceTexture& swapchain, AppContext& context, GPUCommandBuffer command)
{
    if (!swapchain.texture.valid() || !swapchain.view.valid()) {
        return;
    }

    auto device         = context.services.get<GPUDevice*>();
    auto surface        = context.services.try_get<GPUSurface*>();
    auto gui            = context.services.try_get<GUIRenderer*>();
    auto world          = context.services.try_get<World>();
    auto scene_view     = context.services.try_get<SceneView>();
    auto render_world   = context.services.try_get<RenderWorld*>();
    auto render_manager = context.services.try_get<RendererManager*>();

    auto swapchain_format = surface ? surface->get_current_format() : GPUTextureFormat::BGRA8UNORM;

    // construct unified frame graph
    FrameGraphBuilder builder;

    // import swapchain backbuffer target
    FrameGraphTexture imported_swapchain{};
    imported_swapchain.texture = swapchain.texture;
    imported_swapchain.view    = swapchain.view;
    imported_swapchain.format  = swapchain_format;
    imported_swapchain.state   = undefined_texture_state();
    auto swapchain_target      = builder.import(imported_swapchain);

    // check if scene viewport and active renderer are ready to render
    auto scene_backbuffer = scene_view ? scene_view->get_backbuffer() : Backbuffer{};
    bool can_render_scene = render_manager != nullptr &&
                            render_manager->has_active_renderer() &&
                            scene_view != nullptr &&
                            render_world != nullptr &&
                            scene_backbuffer.texture.valid() &&
                            depth_texture.handle.valid() &&
                            scene_backbuffer.extent.width > 0 &&
                            scene_backbuffer.extent.height > 0 &&
                            world != nullptr &&
                            camera_node.entity != entt::null &&
                            world->registry.valid(camera_node.entity) &&
                            world->registry.any_of<Camera>(camera_node.entity) &&
                            world->registry.any_of<TransformWorld>(camera_node.entity);

    FrameGraphHandle<FrameGraphTexture> final_scene_color{};

    if (can_render_scene) {
        auto& cam_comp  = world->get_component<Camera>(camera_node);
        auto& cam_world = world->get_component<TransformWorld>(camera_node);

        RenderView view{};
        view.projection      = cam_comp.projection;
        view.view            = glm::inverse(cam_world.xform);
        view.inv_projection  = glm::inverse(cam_comp.projection);
        view.inv_view        = cam_world.xform;
        view.view_projection = view.projection * view.view;
        view.camera_position = Vector3(cam_world.xform[3]);
        view.render_extent   = scene_backbuffer.extent;
        view.near_plane      = cam_comp.near_plane;
        view.far_plane       = cam_comp.far_plane;
        view.fov             = cam_comp.fov;

        // extract scene representation from RenderWorld
        const auto& scene = render_world->get_scene();

        // import scene canvas color target
        FrameGraphTexture imported_color{};
        imported_color.texture = scene_backbuffer.texture;
        imported_color.view    = scene_backbuffer.texview;
        imported_color.format  = scene_backbuffer.format;
        imported_color.state   = undefined_texture_state();
        auto color_target      = builder.import(imported_color);

        // import scene depth buffer
        FrameGraphTexture imported_depth{};
        imported_depth.texture = depth_texture;
        imported_depth.view    = depth_view;
        imported_depth.format  = GPUTextureFormat::DEPTH32FLOAT;
        imported_depth.state   = undefined_texture_state();
        auto depth_target      = builder.import(imported_depth);

        // pass 1: Scene Rendering (evaluates shadows, skybox/clear, meshes, lights)
        render_manager->resize(scene_backbuffer.extent.width, scene_backbuffer.extent.height);

        RenderFrameOutputs outputs{};
        RenderFrameContext ctx{
            .builder        = builder,
            .view           = view,
            .scene          = scene,
            .color_target   = color_target,
            .depth_target   = depth_target,
            .color_format   = scene_backbuffer.format,
            .depth_format   = GPUTextureFormat::DEPTH32FLOAT,
            .render_extent  = scene_backbuffer.extent,
            .display_extent = scene_backbuffer.extent,
            .outputs        = &outputs,
            .services       = &context.services,
        };

        auto current_color = render_manager->render(ctx);
        auto current_depth = outputs.depth.valid() ? outputs.depth : depth_target;

        // pass 2: composable editor grid pass (depth-tests ground plane against rendered scene depth)
        auto grid_data    = grid_pass.add(builder, current_color, current_depth, view);
        final_scene_color = grid_data.color_output;
    }

    // pass 3: ImGui pass (renders UI to swapchain target and samples the scene color output if available)
    struct ImGuiPassData
    {
        FrameGraphHandle<FrameGraphTexture> output;
        FrameGraphHandle<FrameGraphTexture> scene_texture;
    };

    auto imgui_data = builder.add_pass<ImGuiPassData>(
        "ImGuiPass",
        [&](ImGuiPassData& data, FrameGraphPassBuilder& pass_builder) {
            data.output = pass_builder.render(swapchain_target);
            if (final_scene_color.valid()) {
                data.scene_texture = pass_builder.sample(final_scene_color);
            }
        },
        [gui](const ImGuiPassData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
            if (!gui || !ctx || !ctx->cmdlist.handle.valid()) {
                return;
            }

            auto out_tex = resources.get<FrameGraphTexture>(data.output);
            if (!out_tex) {
                return;
            }

            gui->render_main_viewport(ctx->cmdlist, out_tex->view);
        });

    // pass 4: present transition pass (transitions swapchain backbuffer to present_src state)
    builder.add_pass(
        "PresentPass",
        [&](FrameGraphPassBuilder& pass_builder) {
            (void)pass_builder.present(imgui_data.output);
            pass_builder.preserve();
        },
        [](FrameGraphResources&, FrameGraphContext*) {
            // transition to PRESENT_SRC handled automatically by FrameGraph pre_read
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
