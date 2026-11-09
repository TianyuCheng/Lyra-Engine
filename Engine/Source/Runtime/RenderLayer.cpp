#include <Lyra/Runtime/RenderLayer.h>
#include <Lyra/Utilities/Math.h>
#include <Lyra/Scene/World.h>
#include <Lyra/Components/Camera.h>
#include <Lyra/Components/Light.h>
#include <Lyra/Components/Mesh.h>
#include <Lyra/Components/Transform.h>
#include <Lyra/Compiler/SLCUtils.h>
#include <Lyra/Assets/AMSServer.h>

using namespace lyra;

static void update_camera_projection(Camera& camera)
{
    if (camera.type == ProjectionType::PERSPECTIVE) {
        camera.projection = glm::perspective(
            glm::radians(camera.fov), camera.aspect, camera.near_plane, camera.far_plane);
    } else {
        float half_size   = camera.size * 0.5f;
        camera.projection = glm::ortho(
            -half_size * camera.aspect, half_size * camera.aspect,
            -half_size, half_size, camera.near_plane, camera.far_plane);
    }
}

RenderLayer::RenderLayer()
    : resource_retire(), geometry_cache(resource_retire), renderer_manager(), system_textures()
{
}

void RenderLayer::bind(Application& app)
{
    // query frame count from the surface already registered on the toolboard
    auto surface = app.get_toolboard().get<GPUSurface*>();
    assert(surface != nullptr && "RenderLayer requires a valid GPUSurface registered on the toolboard");

    // initialize deferred resource retirement system
    resource_retire.init(surface->get_image_count());

    // query device and compiler to initialize renderer manager and engine resources
    auto device_ptr   = app.get_toolboard().try_get<GPUDevice*>();
    auto compiler_ptr = app.get_toolboard().try_get<Compiler*>();
    auto ams_ptr      = app.get_toolboard().try_get<AssetServer*>();

    if (device_ptr) {
        GPUDevice device   = *device_ptr;
        Compiler  compiler = compiler_ptr ? *compiler_ptr : Compiler{};
        system_textures.init(device, compiler, ams_ptr);

        auto desc   = RendererDescriptor{};
        desc.device = device;
        if (compiler_ptr) {
            desc.compiler = *compiler_ptr;
        }

        desc.color_format   = surface->get_current_format();
        desc.initial_extent = surface->get_current_extent();

        renderer_manager.init(app.get_context(), desc);
    }

    // register on toolboard so passes, renderers, and systems can resolve them
    app.get_toolboard().add<RenderLayer*>(this);
    app.get_toolboard().add<ResourceRetire*>(&resource_retire);
    app.get_toolboard().add<GeometryCache*>(&geometry_cache);
    app.get_toolboard().add<RendererManager*>(&renderer_manager);
    app.get_toolboard().add<SystemTextures*>(&system_textures);

    // camera projection: runs before rendering
    app.bind<AppEvent::UPDATE_PRE, &RenderLayer::update>(*this);

    // retirement tick: advance retirement queues once per frame before rendering
    app.bind<AppEvent::RENDER_PRE, &RenderLayer::pre_render>(*this);

    // on shutdown: device idle + flush all remaining retirements
    app.bind<AppEvent::DESTROY, &RenderLayer::destroy>(*this);
}

void RenderLayer::update(AppContext& context)
{
    if (auto world = context.toolboard.try_get<World>()) {
        update_camera_projection(*world);
    }
}

void RenderLayer::extract_scene_and_view(AppContext& context)
{
    extracted_scene.clear();

    auto world = context.toolboard.try_get<World>();
    if (!world) {
        return;
    }

    auto ams = context.toolboard.try_get<AssetServer*>();

    // 1. extract camera view parameters
    auto cam_view = world->registry.view<Camera, TransformWorld>();
    for (auto entity : cam_view) {
        auto& cam   = cam_view.get<Camera>(entity);
        auto& xform = cam_view.get<TransformWorld>(entity);

        extracted_view.projection          = cam.projection;
        extracted_view.view                = glm::inverse(xform.xform);
        extracted_view.view_projection     = extracted_view.projection * extracted_view.view;
        extracted_view.inv_view            = xform.xform;
        extracted_view.inv_projection      = glm::inverse(cam.projection);
        extracted_view.inv_view_projection = glm::inverse(extracted_view.view_projection);
        extracted_view.camera_position     = Vector3(xform.xform[3]);
        extracted_view.near_plane          = cam.near_plane;
        extracted_view.far_plane           = cam.far_plane;
        extracted_view.fov                 = cam.fov;
        break; // primary camera
    }

    // 2. extract directional lights
    auto dir_view = world->registry.view<DirectionalLight, TransformWorld>();
    for (auto entity : dir_view) {
        auto& light = dir_view.get<DirectionalLight>(entity);
        auto& xform = dir_view.get<TransformWorld>(entity);

        auto l      = RenderLight{};
        l.type      = RenderLightType::DIRECTIONAL;
        l.direction = glm::normalize(Matrix3x3(xform.xform) * light.direction);
        l.color     = light.color;
        l.intensity = light.intensity;
        extracted_scene.directional_lights.push_back(l);
    }

    // 3. extract point lights
    auto point_view = world->registry.view<PointLight, TransformWorld>();
    for (auto entity : point_view) {
        auto& light = point_view.get<PointLight>(entity);
        auto& xform = point_view.get<TransformWorld>(entity);

        auto l      = RenderLight{};
        l.type      = RenderLightType::POINT;
        l.position  = Vector3(xform.xform[3]) + light.position;
        l.color     = light.color;
        l.intensity = light.intensity;
        l.range     = light.falloff;
        extracted_scene.local_lights.push_back(l);
    }

    // 4. extract spot lights
    auto spot_view = world->registry.view<SpotLight, TransformWorld>();
    for (auto entity : spot_view) {
        auto& light = spot_view.get<SpotLight>(entity);
        auto& xform = spot_view.get<TransformWorld>(entity);

        auto l             = RenderLight{};
        l.type             = RenderLightType::SPOT;
        l.position         = Vector3(xform.xform[3]) + light.position;
        l.direction        = glm::normalize(Matrix3x3(xform.xform) * light.direction);
        l.color            = light.color;
        l.intensity        = light.intensity;
        l.range            = light.falloff;
        l.spot_inner_angle = light.angle.x;
        l.spot_outer_angle = light.angle.y;
        extracted_scene.local_lights.push_back(l);
    }

    // 5. extract mesh primitives
    auto mesh_view = world->registry.view<Mesh, TransformWorld>();
    for (auto entity : mesh_view) {
        auto& mesh_comp = mesh_view.get<Mesh>(entity);
        auto& xform     = mesh_view.get<TransformWorld>(entity);

        auto prim        = MeshPrimitive{};
        prim.material    = mesh_comp.material;
        prim.transform   = xform.xform;
        prim.instance_id = static_cast<uint>(entt::to_integral(entity));

        if (mesh_comp.material.valid() && ams) {
            if (auto mat_ptr = ams->get_asset(mesh_comp.material)) {
                prim.shading_model = static_cast<uint>(mat_ptr->shading_model);
            }
        }

        if (auto entry = geometry_cache.get(mesh_comp.mesh)) {
            if (!entry->lods.empty() && entry->lods[0].traditional) {
                prim.geometry = *entry->lods[0].traditional;
            }
            prim.world_bounds = entry->local_bounds.transformed(xform.xform);
        }

        extracted_scene.meshes.push_back(prim);
    }
}

void RenderLayer::pre_render(AppContext& context)
{
    resource_retire.tick();
    geometry_cache.tick();
    extract_scene_and_view(context);
}

void RenderLayer::drain()
{
    resource_retire.drain();
}

void RenderLayer::destroy(AppContext& context)
{
    renderer_manager.shutdown(context);
    system_textures.shutdown();
    geometry_cache.clear();

    auto device = context.toolboard.get<GPUDevice*>();
    device->wait();
    drain();
}

void RenderLayer::update_camera_projection(World& world)
{
    auto view = world.registry.view<Camera>();
    for (auto entity : view) {
        update_camera_projection(view.get<Camera>(entity));
    }
}

void RenderLayer::update_camera_projection(Camera& camera)
{
    if (camera.type == ProjectionType::PERSPECTIVE) {
        camera.projection = glm::perspective(
            glm::radians(camera.fov), camera.aspect, camera.near_plane, camera.far_plane);
    } else {
        float half_size   = camera.size * 0.5f;
        camera.projection = glm::ortho(
            -half_size * camera.aspect, half_size * camera.aspect,
            -half_size, half_size, camera.near_plane, camera.far_plane);
    }
}
