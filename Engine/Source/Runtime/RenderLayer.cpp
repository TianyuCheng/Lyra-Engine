#include <Lyra/Runtime/RenderLayer.h>
#include <Lyra/Utilities/Logger.h>
#include <Lyra/Utilities/Math.h>
#include <Lyra/Scene/World.h>
#include <Lyra/Components/Camera.h>
#include <Lyra/Components/Light.h>
#include <Lyra/Components/Mesh.h>
#include <Lyra/Components/Transform.h>
#include <Lyra/Compiler/SLCUtils.h>
#include <Lyra/Assets/AMSServer.h>
#include <Lyra/Rendering/ProceduralMesh.h>

using namespace lyra;

namespace
{
    auto get_logger() -> Logger
    {
        static Logger LOGGER = create_logger("RenderLayer", LogLevel::info);
        return LOGGER;
    }
} // namespace

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
    : resource_retire(), geometry_cache(resource_retire), renderer_manager(), system_textures(), render_world(geometry_cache)
{
}

void RenderLayer::bind(Application& app)
{
    // query frame count from the surface already registered on the services
    auto surface = app.get_services().get<GPUSurface*>();
    assert(surface != nullptr && "RenderLayer requires a valid GPUSurface registered on the services");

    // initialize deferred resource retirement system
    resource_retire.init(surface->get_image_count());

    // query device and compiler to initialize renderer manager and engine resources
    auto p_assets   = app.get_services().try_get<AssetServer*>();
    auto p_device   = app.get_services().try_get<GPUDevice*>();
    auto p_compiler = app.get_services().try_get<Compiler*>();

    if (p_assets) {
        render_world.set_asset_server(p_assets);
    }

    if (p_device) {
        GPUDevice device   = *p_device;
        Compiler  compiler = p_compiler ? *p_compiler : Compiler{};
        system_textures.init(device, compiler, p_assets);

        auto desc   = RendererDescriptor{};
        desc.device = device;
        if (p_compiler) {
            desc.compiler = *p_compiler;
        }
        desc.system_textures = &system_textures;

        desc.color_format   = surface->get_current_format();
        desc.initial_extent = surface->get_current_extent();

        renderer_manager.init(app.get_context(), desc);
        init_procedural_meshes(geometry_cache);
    }

    // register on services so passes, renderers, and systems can resolve them
    app.get_services().add<RenderLayer*>(this);
    app.get_services().add<RenderWorld*>(&render_world);
    app.get_services().add<ResourceRetire*>(&resource_retire);
    app.get_services().add<GeometryCache*>(&geometry_cache);
    app.get_services().add<RendererManager*>(&renderer_manager);
    app.get_services().add<SystemTextures*>(&system_textures);
    app.get_services().add<FrameGraphAllocator*>(&renderer_manager.get_allocator());

    // camera projection: runs before rendering
    app.bind<AppEvent::UPDATE_PRE, &RenderLayer::update>(*this);

    // retirement tick: advance retirement queues once per frame before rendering
    app.bind<AppEvent::RENDER_PRE, &RenderLayer::pre_render>(*this);

    // on shutdown: device idle + flush all remaining retirements
    app.bind<AppEvent::DESTROY, &RenderLayer::destroy>(*this);
}

void RenderLayer::update(AppContext& context)
{
    if (auto world = context.services.try_get<World>()) {
        update_camera_projection(*world);
    }
}

void RenderLayer::pre_render(AppContext& context)
{
    resource_retire.tick();
    geometry_cache.tick();

    if (auto world = context.services.try_get<World>()) {
        auto ams = context.services.try_get<AssetServer*>();
        render_world.set_asset_server(ams);

        RendererCapabilities caps{};
        if (renderer_manager.has_active_renderer()) {
            caps = renderer_manager.get_active_api()->get_capabilities();
        } else {
            caps.supported_primitives = PrimitiveSupport::MESH;
        }

        render_world.extract(*world, caps);
    }
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

    if (auto device = context.services.get<GPUDevice*>())
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
