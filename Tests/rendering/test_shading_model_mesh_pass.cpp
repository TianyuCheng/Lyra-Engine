#include <doctest/doctest.h>
#include <Lyra/Rendering/ShadingModelRegistry.h>
#include <Lyra/Rendering/MeshPass.h>
#include <Lyra/Lyra.hpp>

using namespace lyra;

TEST_CASE("rendering::shading_model_registry" * doctest::description("Verify equal treatment of builtin and custom shading models"))
{
    static_assert(std::is_same_v<ShadingModelID, GUID>, "ShadingModelID must be an alias of GUID");

    ShadingModelRegistry::init();

    SUBCASE("Builtin models are queryable as first-class citizens")
    {
        auto standard_opt = ShadingModelRegistry::get_model(BuiltinShadingModels::STANDARD);
        REQUIRE(standard_opt.has_value());
        CHECK_EQ(standard_opt->name, "Standard");
        CHECK_EQ(standard_opt->shader_module_path, "sys://Shaders/Forward/Forward.slang");
        CHECK_EQ(standard_opt->entry_point_name, "fs_standard");
        CHECK_EQ(standard_opt->struct_name, "StandardShadingModel");

        auto clearcoat_opt = ShadingModelRegistry::get_model(BuiltinShadingModels::CLEARCOAT);
        REQUIRE(clearcoat_opt.has_value());
        CHECK_EQ(clearcoat_opt->name, "Clearcoat");
        CHECK_EQ(clearcoat_opt->shader_module_path, "sys://Shaders/Forward/Forward.slang");
        CHECK_EQ(clearcoat_opt->entry_point_name, "fs_clearcoat");
        CHECK_EQ(clearcoat_opt->struct_name, "ClearcoatShadingModel");

        auto subsurface_opt = ShadingModelRegistry::get_model(BuiltinShadingModels::SUBSURFACE);
        REQUIRE(subsurface_opt.has_value());
        CHECK_EQ(subsurface_opt->name, "Subsurface");
        CHECK_EQ(subsurface_opt->shader_module_path, "sys://Shaders/Forward/Forward.slang");
        CHECK_EQ(subsurface_opt->entry_point_name, "fs_subsurface");
        CHECK_EQ(subsurface_opt->struct_name, "SubsurfaceShadingModel");

        auto unlit_opt = ShadingModelRegistry::get_model(BuiltinShadingModels::UNLIT);
        REQUIRE(unlit_opt.has_value());
        CHECK_EQ(unlit_opt->name, "Unlit");
        CHECK_EQ(unlit_opt->shader_module_path, "sys://Shaders/Forward/Forward.slang");
        CHECK_EQ(unlit_opt->entry_point_name, "fs_unlit");
        CHECK_EQ(unlit_opt->struct_name, "UnlitShadingModel");
    }

    SUBCASE("Custom user shading models register identically without distinction")
    {
        constexpr ShadingModelID CUSTOM_GUID = 0x9876543210ULL;
        ShadingModelInfo         custom_info{
            .id                 = CUSTOM_GUID,
            .name               = "Hologram",
            .shader_module_path = "app://Shaders/Hologram.slang",
            .struct_name        = "HologramShadingModel",
        };

        ShadingModelRegistry::register_model(custom_info);

        auto retrieved = ShadingModelRegistry::get_model(CUSTOM_GUID);
        REQUIRE(retrieved.has_value());
        CHECK_EQ(retrieved->name, "Hologram");
        CHECK_EQ(retrieved->shader_module_path, "app://Shaders/Hologram.slang");
        CHECK_EQ(retrieved->struct_name, "HologramShadingModel");

        auto all = ShadingModelRegistry::all_models();
        CHECK(all.size() >= 4);
    }

    SUBCASE("from_enum maps legacy enum to unified ShadingModelID")
    {
        CHECK_EQ(BuiltinShadingModels::from_enum(ShadingModel::STANDARD), BuiltinShadingModels::STANDARD);
        CHECK_EQ(BuiltinShadingModels::from_enum(ShadingModel::CLEARCOAT), BuiltinShadingModels::CLEARCOAT);
        CHECK_EQ(BuiltinShadingModels::from_enum(ShadingModel::SUBSURFACE), BuiltinShadingModels::SUBSURFACE);
        CHECK_EQ(BuiltinShadingModels::from_enum(ShadingModel::UNLIT), BuiltinShadingModels::UNLIT);
    }
}

TEST_CASE("rendering::render_sort_key" * doctest::description("Verify bitwise sorting and state-change minimization"))
{
    SUBCASE("Opaque sorting minimizes pipeline changes, then buffer changes, then front-to-back depth")
    {
        // pipeline 1 vs pipeline 2
        auto key_p1 = RenderSortKey::make(1, 10, 0.9f, false);
        auto key_p2 = RenderSortKey::make(2, 5, 0.1f, false);
        CHECK(key_p1 < key_p2);

        // same pipeline, buffer 1 vs buffer 2
        auto key_b1 = RenderSortKey::make(1, 10, 0.9f, false);
        auto key_b2 = RenderSortKey::make(1, 20, 0.1f, false);
        CHECK(key_b1 < key_b2);

        // same pipeline and buffer, front-to-back depth
        auto key_near = RenderSortKey::make(1, 10, 0.2f, false);
        auto key_far  = RenderSortKey::make(1, 10, 0.8f, false);
        CHECK(key_near < key_far);
    }

    SUBCASE("Translucent sorting sorts back-to-front depth")
    {
        auto key_near = RenderSortKey::make(1, 10, 0.2f, true);
        auto key_far  = RenderSortKey::make(1, 10, 0.8f, true);
        CHECK(key_far < key_near);
    }
}

TEST_CASE("rendering::mesh_pass_pipeline_resolver" * doctest::description("Verify MeshPass delegates pipeline resolution to caller without manager coupling"))
{
    MeshPass pass;

    RenderScene   scene;
    MeshPrimitive prim{};
    prim.geometry.index_count         = 6;
    prim.geometry.vertex_slice.buffer = GPUBufferHandle{1};
    prim.geometry.vertex_slice.size   = 1024;
    prim.geometry.index_slice.buffer  = GPUBufferHandle{2};
    prim.geometry.index_slice.size    = 24;
    prim.index_count                  = 6;
    prim.shading_model_id             = BuiltinShadingModels::STANDARD;
    scene.meshes.push_back(prim);

    RenderView view{};
    view.camera_position = Vector3(0.0f, 0.0f, 5.0f);

    uint resolve_called = 0;
    pass.prepare(GPUDevice{}, scene, view, [&](const MeshPrimitive& mesh) -> GPURenderPipeline {
        resolve_called++;
        CHECK_EQ(mesh.shading_model_id, BuiltinShadingModels::STANDARD);
        GPURenderPipeline pipe{};
        pipe.handle = GPURenderPipelineHandle{42};
        return pipe;
    });

    CHECK_EQ(resolve_called, 1);
    CHECK_EQ(pass.get_draw_call_count(), 1);
    CHECK_EQ(pass.get_triangle_count(), 2);
}

TEST_CASE("rendering::shading_model_variant_compilation" * doctest::description("Verify compilation and reflection across specialized shading model variants without module collisions"))
{
    FileLoader loader(FSLoader::NATIVE);
    (void)loader.mount("sys://", "Engine/Assets", 0);

    static CString     includes[] = {"Engine/Assets/Shaders"};
    CompilerDescriptor cdesc{};
    cdesc.target    = CompileTarget::SPIRV;
    cdesc.includes  = includes;
    cdesc.fs_loader = &loader;
    auto compiler   = Compiler::init(cdesc);
    REQUIRE(compiler != nullptr);

    // 1. Compile consolidated Forward.slang module
    CompileDescriptor desc{};
    desc.module = "Forward";
    desc.path   = "sys://Shaders/Forward/Forward.slang";
    auto mod    = compiler->compile(desc);
    REQUIRE(mod.has_value());

    // 2. Reflect specialized Standard variant
    auto refl_standard = compiler->reflect({
        {*mod, "vsmain"},
        {*mod, "fs_standard"},
    });
    REQUIRE(refl_standard.has_value());
    CHECK_EQ(*(*refl_standard)->get_immediate_size(), sizeof(MeshDrawInstance));
    CHECK_EQ(sizeof(MeshDrawInstance), 112);
    auto layouts = (*refl_standard)->get_bind_group_layouts();
    REQUIRE(layouts.has_value());
    REQUIRE_EQ(layouts->size(), 2);
    CHECK_EQ((*layouts)[0].entries.size(), 9); // Camera, shadow map, shadow sampler, cluster grid, light index, lights, env_map, brdf_lut, env_sampler in set 0
    CHECK_EQ((*layouts)[1].entries.size(), 1); // Vertex buffer in set 1

    // 3. Reflect specialized Clearcoat variant
    auto refl_clearcoat = compiler->reflect({
        {*mod, "vsmain"},
        {*mod, "fs_clearcoat"},
    });
    REQUIRE(refl_clearcoat.has_value());

    // 4. Reflect specialized Subsurface variant
    auto refl_subsurface = compiler->reflect({
        {*mod, "vsmain"},
        {*mod, "fs_subsurface"},
    });
    REQUIRE(refl_subsurface.has_value());

    // 5. Reflect specialized Unlit variant
    auto refl_unlit = compiler->reflect({
        {*mod, "vsmain"},
        {*mod, "fs_unlit"},
    });
    REQUIRE(refl_unlit.has_value());

    // 5. Compile and reflect Skybox shader
    CompileDescriptor sky_desc{};
    sky_desc.module = "Skybox";
    sky_desc.path   = "sys://Shaders/Sky/Skybox.slang";
    auto sky_mod    = compiler->compile(sky_desc);
    REQUIRE(sky_mod.has_value());

    auto refl_sky = compiler->reflect({
        {*sky_mod, "vsmain"},
        {*sky_mod, "fsmain"},
    });
    REQUIRE(refl_sky.has_value());

    // 6. Compile Clouds shader module
    CompileDescriptor cloud_desc{};
    cloud_desc.module = "Clouds";
    cloud_desc.path   = "sys://Shaders/Sky/Clouds.slang";
    auto cloud_mod    = compiler->compile(cloud_desc);
    REQUIRE(cloud_mod.has_value());
}
