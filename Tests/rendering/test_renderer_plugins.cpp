#include <doctest/doctest.h>
#include <Lyra/Rendering/RendererAPI.h>

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

        RendererHandle handle{};
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

        RendererHandle handle{};
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

        RendererHandle handle{};
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

        RendererHandle handle{};
        RendererDescriptor desc{};
        CHECK(api->create_renderer(handle, desc));
        CHECK(handle.valid());

        api->delete_renderer(handle);
    }
}
