#include <doctest/doctest.h>
#include <Lyra/Rendering/RendererAPI.h>
#include <Lyra/Rendering/ProceduralMesh.h>
#include <Lyra/Rendering/RenderWorld.h>
#include <Lyra/Rendering/ResourceRetire.h>
#include <Lyra/Components/Camera.h>
#include <Lyra/Components/Light.h>
#include <Lyra/Components/Mesh.h>
#include <Lyra/Components/SDFVolume.h>

using namespace lyra;

TEST_CASE("rendering::renderer_plugins" * doctest::description("Renderer plugin dynamic loading and capability verification"))
{
    SUBCASE("Load and verify lyra-renderer-forward")
    {
        RendererPlugin plugin;
        REQUIRE(plugin.load("lyra-renderer-forward"));

        auto api = plugin.get_api();
        REQUIRE(api != nullptr);
        REQUIRE(api->get_name != nullptr);
        CHECK(StringView(api->get_name()).find("Forward") != StringView::npos);

        REQUIRE(api->get_capabilities != nullptr);
        auto caps = api->get_capabilities();
        CHECK(caps.features.contains(RendererFeature::RASTERIZATION));
        CHECK(caps.supported_primitives.contains(PrimitiveSupport::MESH));

        REQUIRE(api->create_renderer != nullptr);
        REQUIRE(api->delete_renderer != nullptr);

        RendererHandle     handle{};
        RendererDescriptor desc{};
        CHECK(api->create_renderer(handle, desc));
        CHECK(handle.valid());

        api->delete_renderer(handle);
    }

    SUBCASE("Load and verify lyra-renderer-gpu-driven")
    {
        RendererPlugin plugin;
        REQUIRE(plugin.load("lyra-renderer-gpu-driven"));

        auto api = plugin.get_api();
        REQUIRE(api != nullptr);
        REQUIRE(api->get_name != nullptr);
        CHECK(StringView(api->get_name()).find("GPU Driven") != StringView::npos);

        REQUIRE(api->get_capabilities != nullptr);
        auto caps = api->get_capabilities();
        CHECK(caps.features.contains(RendererFeature::COMPUTE_CULLING));
        CHECK(caps.supported_primitives.contains(PrimitiveSupport::MESHLET));

        RendererHandle     handle{};
        RendererDescriptor desc{};
        CHECK(api->create_renderer(handle, desc));
        CHECK(handle.valid());

        api->delete_renderer(handle);
    }

    SUBCASE("Load and verify lyra-renderer-deferred")
    {
        RendererPlugin plugin;
        REQUIRE(plugin.load("lyra-renderer-deferred"));

        auto api = plugin.get_api();
        REQUIRE(api != nullptr);
        REQUIRE(api->get_name != nullptr);
        CHECK(StringView(api->get_name()).find("Deferred") != StringView::npos);

        REQUIRE(api->get_capabilities != nullptr);
        auto caps = api->get_capabilities();
        CHECK(caps.features.contains(RendererFeature::HARDWARE_RAYTRACING));
        CHECK(caps.supported_primitives.contains(PrimitiveSupport::BVH));

        RendererHandle     handle{};
        RendererDescriptor desc{};
        CHECK(api->create_renderer(handle, desc));
        CHECK(handle.valid());

        api->delete_renderer(handle);
    }

    SUBCASE("Load and verify lyra-renderer-pathtracer")
    {
        RendererPlugin plugin;
        REQUIRE(plugin.load("lyra-renderer-pathtracer"));

        auto api = plugin.get_api();
        REQUIRE(api != nullptr);
        REQUIRE(api->get_name != nullptr);
        CHECK(StringView(api->get_name()).find("PathTracer") != StringView::npos);

        REQUIRE(api->get_capabilities != nullptr);
        auto caps = api->get_capabilities();
        CHECK(caps.features.contains(RendererFeature::ACCUMULATION));
        CHECK(caps.supported_primitives.contains(PrimitiveSupport::BVH));

        RendererHandle     handle{};
        RendererDescriptor desc{};
        CHECK(api->create_renderer(handle, desc));
        CHECK(handle.valid());

        api->delete_renderer(handle);
    }
}

TEST_CASE("rendering::procedural_mesh" * doctest::description("Verify generation of procedural mesh primitives including icosphere"))
{
    SUBCASE("Procedural mesh lookup via find_procedural_mesh")
    {
        CHECK(find_procedural_mesh(PROCEDURAL_CUBE_GUID) != nullptr);
        CHECK(find_procedural_mesh(PROCEDURAL_SPHERE_GUID) != nullptr);
        CHECK(find_procedural_mesh(PROCEDURAL_PLANE_GUID) != nullptr);
        CHECK(find_procedural_mesh(PROCEDURAL_CYLINDER_GUID) != nullptr);
        CHECK(find_procedural_mesh(PROCEDURAL_ICOSPHERE_GUID) != nullptr);
        CHECK(find_procedural_mesh(0xDEADBEEFULL) == nullptr);
    }

    SUBCASE("Icosphere mesh generation")
    {
        auto ico0 = create_icosphere_mesh(1.0f, 0);
        REQUIRE_EQ(ico0.lods.size(), 1);
        CHECK_EQ(ico0.lods[0].surfaces[0].slice.index_count, 60); // 20 faces * 3
        CHECK_EQ(ico0.lods[0].attributes[0].element_count, 12);   // 12 base vertices

        auto ico1 = create_icosphere_mesh(2.0f, 1);
        REQUIRE_EQ(ico1.lods.size(), 1);
        CHECK_EQ(ico1.lods[0].surfaces[0].slice.index_count, 240); // 20 * 4 * 3
        CHECK_EQ(ico1.lods[0].attributes[0].element_count, 42);    // 12 + 30 midpoints
        CHECK_EQ(ico1.min_bounds.x, doctest::Approx(-2.0f));
        CHECK_EQ(ico1.max_bounds.x, doctest::Approx(2.0f));

        auto ico2 = create_icosphere_mesh(0.5f, 2);
        REQUIRE_EQ(ico2.lods.size(), 1);
        CHECK_EQ(ico2.lods[0].surfaces[0].slice.index_count, 960); // 240 * 4
        CHECK_GT(ico2.lods[0].attributes[0].element_count, 42);
        CHECK_EQ(ico2.lods[0].surfaces.size(), 1);
        CHECK_EQ(ico2.lods[0].surfaces[0].slice.index_count, 960);
    }

    SUBCASE("Cube, UV Sphere, Plane, Cylinder mesh generation")
    {
        auto cube = create_cube_mesh(1.0f);
        CHECK_EQ(cube.lods[0].surfaces[0].slice.index_count, 36);
        CHECK_EQ(cube.lods[0].attributes[0].element_count, 24);

        auto sphere = create_sphere_mesh(1.0f, 8, 8);
        CHECK_GT(sphere.lods[0].surfaces[0].slice.index_count, 0);

        auto plane = create_plane_mesh(5.0f, 5.0f, 2, 2);
        CHECK_EQ(plane.lods[0].surfaces[0].slice.index_count, 24);

        auto cyl = create_cylinder_mesh(1.0f, 2.0f, 12);
        CHECK_GT(cyl.lods[0].surfaces[0].slice.index_count, 0);
    }
}

TEST_CASE("rendering::render_world" * doctest::description("Verify RenderWorld multi-form geometry extraction and ECS synchronization"))
{
    World          ecs_world;
    ResourceRetire retire;
    GeometryCache  cache(retire);
    RenderWorld    render_world(cache);

    SUBCASE("Extract view, directional lights, and point lights")
    {
        auto  cam_entity = ecs_world.create("Camera");
        auto& cam        = ecs_world.add_component<Camera>(cam_entity);
        cam.fov          = 60.0f;
        cam.near_plane   = 0.1f;
        cam.far_plane    = 1000.0f;

        auto  light_entity = ecs_world.create("Sun");
        auto& dir          = ecs_world.add_component<DirectionalLight>(light_entity);
        dir.color          = Vector3(1.0f, 0.95f, 0.9f);
        dir.intensity      = 5.0f;

        RendererCapabilities caps{};
        render_world.extract(ecs_world, caps);

        CHECK_EQ(render_world.get_scene().directional_lights.size(), 1);
        CHECK_EQ(render_world.get_scene().directional_lights[0].intensity, 5.0f);
        CHECK_EQ(render_world.get_view().fov, 60.0f);
        CHECK_EQ(render_world.get_view().near_plane, 0.1f);
    }

    SUBCASE("Extract procedural meshes (Trimesh form)")
    {
        auto  cube_entity = ecs_world.create("TestCube");
        auto& cube_mesh   = ecs_world.add_component<Mesh>(cube_entity);
        cube_mesh.mesh    = MeshAssetHandle(PROCEDURAL_CUBE_GUID);

        auto  ico_entity = ecs_world.create("TestIcosphere");
        auto& ico_mesh   = ecs_world.add_component<Mesh>(ico_entity);
        ico_mesh.mesh    = MeshAssetHandle(PROCEDURAL_ICOSPHERE_GUID);

        RendererCapabilities caps{};
        caps.supported_primitives = PrimitiveSupport::MESH;
        render_world.extract(ecs_world, caps);

        CHECK_EQ(render_world.get_scene().meshes.size(), 2);
    }

    SUBCASE("Extract SDF volume (SDF form)")
    {
        auto  sdf_entity = ecs_world.create("TestSDFSphere");
        auto& sdf        = ecs_world.add_component<SDFVolume>(sdf_entity);
        sdf.shape_type   = SDFShapeType::SPHERE;
        sdf.blend_op     = SDFBlendOp::SMOOTH_UNION;
        sdf.parameters   = Vector4(2.5f, 0.0f, 0.0f, 0.2f);

        RendererCapabilities caps{};
        caps.supported_primitives = PrimitiveSupport::SDF;
        render_world.extract(ecs_world, caps);

        CHECK_EQ(render_world.get_scene().sdfs.size(), 1);
        CHECK_EQ(render_world.get_scene().sdfs[0].shape_type, SDFShapeType::SPHERE);
        CHECK_EQ(render_world.get_scene().sdfs[0].blend_op, SDFBlendOp::SMOOTH_UNION);
        CHECK_EQ(render_world.get_scene().sdfs[0].parameters.x, 2.5f);
    }
}
