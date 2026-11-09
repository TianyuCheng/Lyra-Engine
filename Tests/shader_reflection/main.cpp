#include "helper.h"
#include <algorithm>
#include <cstring>

void test_shader_vertex_attribute_reflection(CompileTarget target, CompileFlags flags)
{
    String code = R"""(
    import lyra;

    struct VertexInput
    {
        float3 position : POSITION;
        float2 texcoord : TEXCOORD0;
        float3 color    : TEXCOORD1;
    };

    struct VertexOutput
    {
        float4 position : SV_POSITION;
        float3 color    : TEXCOORD1;
    };

    struct Camera
    {
        float4x4 proj;
        float4x4 view;
    };

    struct Xform
    {
        float3 data;
        float4x4 mvp;
    };

    struct Hello
    {
        ConstantBuffer<Camera> cam;
        Texture2D<float4> tex;
        Texture2D<float4> tex2;
        SamplerState smp;
    };

    [[lyra::immediates]]
    ConstantBuffer<Xform> xform1;

    [[lyra::group(0)]]
    ParameterBlock<Hello> haha;

    [[lyra::group(1)]]
    ParameterBlock<Hello> hihi;

    [shader("vertex")]
    VertexOutput vsmain(VertexInput input)
    {
        VertexOutput output;
        output.color = input.color;
        output.position = float4(input.position, 1.0);
        output.position = mul(output.position, xform1.mvp);
        output.position = mul(output.position, haha.cam.view);
        output.position = mul(output.position, hihi.cam.proj);
        return output;
    }

    [shader("fragment")]
    float4 fsmain(VertexOutput input) : SV_TARGET
    {
        return haha.tex.Sample(haha.smp, input.color.xy) +
               hihi.tex.Sample(haha.smp, input.color.xy);
    }
    )""";

    // initialize compiler
    auto compiler = execute([&]() {
        auto desc      = CompilerDescriptor{};
        desc.target    = target;
        desc.flags     = flags;
        desc.log_level = LogLevel::info;
        return Compiler::init(desc);
    });

    auto module = execute([&]() {
        auto desc   = CompileDescriptor{};
        desc.module = "test";
        desc.path   = "test.slang";
        desc.source = code.c_str();
        return compiler->compile(desc);
    });

    auto reflection = compiler->reflect({
        {*module, "vsmain"},
        {*module, "fsmain"},
    });

    // define a dummy Vertex struct for offsetof to work,
    // this should match the struct used by the calling code.
    struct Vertex
    {
        float position[3];
        float uv[2];
    };

    // reflect vertex attributes
    auto attributes = reflection->get_vertex_attributes({
        {"position", offsetof(Vertex, position)},
        {"texcoord", offsetof(Vertex, uv)},
    });

    CHECK_EQ(attributes.size(), 2);

    // attribute: position
    auto pos_attrib_it = std::find_if(attributes.begin(), attributes.end(), [](const auto& a) {
        return strcmp(a.shader_semantic, "POSITION") == 0;
    });
    CHECK(pos_attrib_it != attributes.end());
    if (pos_attrib_it != attributes.end()) {
        switch (target) {
            case CompileTarget::DXIL:
                CHECK_EQ(String(pos_attrib_it->shader_semantic), "POSITION");
                CHECK_EQ(pos_attrib_it->shader_location, 0);
                break;
            default:
                CHECK_EQ(pos_attrib_it->shader_location, 0);
                break;
        }
        CHECK_EQ(pos_attrib_it->offset, offsetof(Vertex, position));
    }

    // attribute: texcoord
    auto tex_attrib_it = std::find_if(attributes.begin(), attributes.end(), [](const auto& a) {
        return strcmp(a.shader_semantic, "TEXCOORD") == 0;
    });
    CHECK(tex_attrib_it != attributes.end());
    if (tex_attrib_it != attributes.end()) {
        switch (target) {
            case CompileTarget::DXIL:
                CHECK_EQ(String(tex_attrib_it->shader_semantic), "TEXCOORD");
                CHECK_EQ(tex_attrib_it->shader_location, 0);
                break;
            default:
                CHECK_EQ(tex_attrib_it->shader_location, 1);
                break;
        }
        CHECK_EQ(tex_attrib_it->offset, offsetof(Vertex, uv));
    }

    auto check_binding_index = [&](const GPUBindingIndex& binding, uint vulkan_index, uint metal_index, uint d3d_index) {
        switch (target) {
            case CompileTarget::MSL:
                CHECK_EQ(binding.index, metal_index);
                break;
            case CompileTarget::DXIL:
                CHECK_EQ(binding.index, d3d_index);
                break;
            case CompileTarget::SPIRV:
                CHECK_EQ(binding.index, vulkan_index);
                break;
        }
    };

    auto bindgroups = reflection->get_bind_group_layouts();
    CHECK_EQ(bindgroups.size(), 2);

    // haha
    auto haha_bindgroup_it = std::find_if(bindgroups.begin(), bindgroups.end(), [](const auto& a) {
        return strcmp(a.label, "haha") == 0;
    });
    CHECK(haha_bindgroup_it != bindgroups.end());
    if (haha_bindgroup_it != bindgroups.end()) {
        CHECK_EQ(haha_bindgroup_it->entries.size(), 4);

        // cam (used in vertex)
        CHECK_EQ(haha_bindgroup_it->entries.at(0).type, GPUResourceType::BUFFER);
        CHECK_EQ(haha_bindgroup_it->entries.at(0).buffer.type, GPUBufferBindingType::UNIFORM);
        CHECK_EQ(haha_bindgroup_it->entries.at(0).count, 1);
        check_binding_index(haha_bindgroup_it->entries.at(0).binding, 0, 0, 0);
        if (target != CompileTarget::MSL) {
            CHECK(haha_bindgroup_it->entries.at(0).visibility.contains(GPUShaderStage::VERTEX));
            CHECK(!haha_bindgroup_it->entries.at(0).visibility.contains(GPUShaderStage::FRAGMENT));
        } else {
            CHECK(haha_bindgroup_it->entries.at(0).binding.from_argument_buffer);
        }

        // tex (used in fragment)
        CHECK_EQ(haha_bindgroup_it->entries.at(1).type, GPUResourceType::TEXTURE);
        CHECK_EQ(haha_bindgroup_it->entries.at(1).count, 1);
        check_binding_index(haha_bindgroup_it->entries.at(1).binding, 1, 1, 0);
        if (target != CompileTarget::MSL) {
            CHECK(!haha_bindgroup_it->entries.at(1).visibility.contains(GPUShaderStage::VERTEX));
            CHECK(haha_bindgroup_it->entries.at(1).visibility.contains(GPUShaderStage::FRAGMENT));
        } else {
            CHECK(haha_bindgroup_it->entries.at(0).binding.from_argument_buffer);
        }

        // tex2 (not used)
        CHECK_EQ(haha_bindgroup_it->entries.at(2).type, GPUResourceType::TEXTURE);
        CHECK_EQ(haha_bindgroup_it->entries.at(2).count, 1);
        check_binding_index(haha_bindgroup_it->entries.at(2).binding, 2, 2, 1);
        if (target != CompileTarget::MSL) {
            CHECK(!haha_bindgroup_it->entries.at(2).visibility.contains(GPUShaderStage::VERTEX));
            CHECK(!haha_bindgroup_it->entries.at(2).visibility.contains(GPUShaderStage::FRAGMENT));
        } else {
            CHECK(haha_bindgroup_it->entries.at(0).binding.from_argument_buffer);
        }

        // smp (used in fragment)
        CHECK_EQ(haha_bindgroup_it->entries.at(3).type, GPUResourceType::SAMPLER);
        CHECK_EQ(haha_bindgroup_it->entries.at(3).count, 1);
        check_binding_index(haha_bindgroup_it->entries.at(3).binding, 3, 3, 0);
        if (target != CompileTarget::MSL) {
            CHECK(!haha_bindgroup_it->entries.at(3).visibility.contains(GPUShaderStage::VERTEX));
            CHECK(haha_bindgroup_it->entries.at(3).visibility.contains(GPUShaderStage::FRAGMENT));
        } else {
            CHECK(haha_bindgroup_it->entries.at(0).binding.from_argument_buffer);
        }
    }

    // hihi
    auto hihi_bindgroup_it = std::find_if(bindgroups.begin(), bindgroups.end(), [](const auto& a) {
        return strcmp(a.label, "hihi") == 0;
    });
    CHECK(hihi_bindgroup_it != bindgroups.end());
    if (hihi_bindgroup_it != bindgroups.end()) {
        CHECK_EQ(hihi_bindgroup_it->entries.size(), 4);

        // cam (used in vertex)
        CHECK_EQ(hihi_bindgroup_it->entries.at(0).type, GPUResourceType::BUFFER);
        CHECK_EQ(hihi_bindgroup_it->entries.at(0).buffer.type, GPUBufferBindingType::UNIFORM);
        CHECK_EQ(hihi_bindgroup_it->entries.at(0).count, 1);
        check_binding_index(haha_bindgroup_it->entries.at(0).binding, 0, 0, 0);
        if (target != CompileTarget::MSL) {
            CHECK(hihi_bindgroup_it->entries.at(0).visibility.contains(GPUShaderStage::VERTEX));
            CHECK(!hihi_bindgroup_it->entries.at(0).visibility.contains(GPUShaderStage::FRAGMENT));
        } else {
            CHECK(hihi_bindgroup_it->entries.at(0).binding.from_argument_buffer);
        }

        // tex (used in fragment)
        CHECK_EQ(hihi_bindgroup_it->entries.at(1).type, GPUResourceType::TEXTURE);
        CHECK_EQ(hihi_bindgroup_it->entries.at(1).count, 1);
        check_binding_index(haha_bindgroup_it->entries.at(1).binding, 1, 1, 0);
        if (target != CompileTarget::MSL) {
            CHECK(!hihi_bindgroup_it->entries.at(1).visibility.contains(GPUShaderStage::VERTEX));
            CHECK(hihi_bindgroup_it->entries.at(1).visibility.contains(GPUShaderStage::FRAGMENT));
        } else {
            CHECK(hihi_bindgroup_it->entries.at(0).binding.from_argument_buffer);
        }

        // tex2 (not used)
        CHECK_EQ(haha_bindgroup_it->entries.at(2).type, GPUResourceType::TEXTURE);
        CHECK_EQ(haha_bindgroup_it->entries.at(2).count, 1);
        check_binding_index(haha_bindgroup_it->entries.at(2).binding, 2, 2, 1);
        if (target != CompileTarget::MSL) {
            CHECK(!haha_bindgroup_it->entries.at(2).visibility.contains(GPUShaderStage::VERTEX));
            CHECK(!haha_bindgroup_it->entries.at(2).visibility.contains(GPUShaderStage::FRAGMENT));
        } else {
            CHECK(hihi_bindgroup_it->entries.at(0).binding.from_argument_buffer);
        }

        // smp (not used)
        CHECK_EQ(hihi_bindgroup_it->entries.at(3).type, GPUResourceType::SAMPLER);
        CHECK_EQ(hihi_bindgroup_it->entries.at(3).count, 1);
        check_binding_index(haha_bindgroup_it->entries.at(3).binding, 3, 3, 0);
        if (target != CompileTarget::MSL) {
            CHECK(!hihi_bindgroup_it->entries.at(3).visibility.contains(GPUShaderStage::VERTEX));
            CHECK(!hihi_bindgroup_it->entries.at(3).visibility.contains(GPUShaderStage::FRAGMENT));
        } else {
            CHECK(hihi_bindgroup_it->entries.at(0).binding.from_argument_buffer);
        }
    }

    auto immediate_size = reflection->get_immediate_size();
    CHECK_EQ(immediate_size, 80);
}

void test_shader_explicit_group_and_binding_reflection(CompileTarget target, CompileFlags flags)
{
    String code = R"""(
    import lyra;

    struct Camera
    {
        float4x4 proj;
        float4x4 view;
    };

    struct Material
    {
        [[lyra::binding(5)]]
        Texture2D<float4> tex;

        [[lyra::binding(2)]]
        SamplerState smp;
    };

    struct Xform
    {
        float4x4 mvp;
    };

    [[lyra::immediates]]
    ConstantBuffer<Xform> xform;

    // declared out of order: material is explicitly group 2, scene is explicitly group 0
    [[lyra::group(2)]]
    ParameterBlock<Material> material;

    [[lyra::group(0)]]
    ParameterBlock<Camera> scene;

    struct VertexInput
    {
        float3 position : POSITION;
    };

    struct VertexOutput
    {
        float4 position : SV_POSITION;
    };

    [shader("vertex")]
    VertexOutput vsmain(VertexInput input)
    {
        VertexOutput output;
        output.position = float4(input.position, 1.0);
        output.position = mul(output.position, xform.mvp);
        output.position = mul(output.position, scene.view);
        return output;
    }

    [shader("fragment")]
    float4 fsmain(VertexOutput input) : SV_TARGET
    {
        return material.tex.Sample(material.smp, float2(0, 0));
    }
    )""";

    auto compiler = execute([&]() {
        auto desc      = CompilerDescriptor{};
        desc.target    = target;
        desc.flags     = flags;
        desc.log_level = LogLevel::info;
        return Compiler::init(desc);
    });

    auto module = execute([&]() {
        auto desc   = CompileDescriptor{};
        desc.module = "test_explicit";
        desc.path   = "test_explicit.slang";
        desc.source = code.c_str();
        return compiler->compile(desc);
    });

    auto reflection = compiler->reflect({
        {*module, "vsmain"},
        {*module, "fsmain"},
    });

    uint material_group = reflection->get_bind_group_location("material");
    CHECK_EQ(material_group, 2);

    uint scene_group = reflection->get_bind_group_location("scene");
    CHECK_EQ(scene_group, 0);

    auto bindgroups = reflection->get_bind_group_layouts();
    CHECK_EQ(bindgroups.size(), 2);

    // verify material group (group 2) and its explicit bindings
    auto mat_bg_it = std::find_if(bindgroups.begin(), bindgroups.end(), [](const auto& bg) {
        return bg.label && strcmp(bg.label, "material") == 0;
    });
    CHECK(mat_bg_it != bindgroups.end());
    if (mat_bg_it != bindgroups.end()) {
        CHECK_EQ(mat_bg_it->entries.size(), 2);

        auto tex_it = std::find_if(mat_bg_it->entries.begin(), mat_bg_it->entries.end(), [](const auto& e) {
            return e.type == GPUResourceType::TEXTURE;
        });
        CHECK(tex_it != mat_bg_it->entries.end());
        if (tex_it != mat_bg_it->entries.end()) {
            CHECK_EQ(tex_it->binding.index, 5);
        }

        auto smp_it = std::find_if(mat_bg_it->entries.begin(), mat_bg_it->entries.end(), [](const auto& e) {
            return e.type == GPUResourceType::SAMPLER;
        });
        CHECK(smp_it != mat_bg_it->entries.end());
        if (smp_it != mat_bg_it->entries.end()) {
            CHECK_EQ(smp_it->binding.index, 2);
        }
    }

    auto immediate_size = reflection->get_immediate_size();
    CHECK_EQ(immediate_size, 64);
}

void test_shader_explicit_two_arg_binding_reflection(CompileTarget target, CompileFlags flags)
{
    String code = R"""(
    import lyra;

    [[lyra::binding(7, 3)]]
    Texture2D<float4> lut;

    [[lyra::binding(1, 3)]]
    SamplerState smp;

    struct VertexInput
    {
        float3 position : POSITION;
    };

    struct VertexOutput
    {
        float4 position : SV_POSITION;
    };

    [shader("vertex")]
    VertexOutput vsmain(VertexInput input)
    {
        VertexOutput output;
        output.position = float4(input.position, 1.0);
        return output;
    }

    [shader("fragment")]
    float4 fsmain(VertexOutput input) : SV_TARGET
    {
        return lut.Sample(smp, float2(0, 0));
    }
    )""";

    auto compiler = execute([&]() {
        auto desc      = CompilerDescriptor{};
        desc.target    = target;
        desc.flags     = flags;
        desc.log_level = LogLevel::info;
        return Compiler::init(desc);
    });

    auto module = execute([&]() {
        auto desc   = CompileDescriptor{};
        desc.module = "test_two_arg_binding";
        desc.path   = "test_two_arg_binding.slang";
        desc.source = code.c_str();
        return compiler->compile(desc);
    });

    auto reflection = compiler->reflect({
        {*module, "vsmain"},
        {*module, "fsmain"},
    });

    auto bindgroups = reflection->get_bind_group_layouts();
    CHECK_EQ(bindgroups.size(), 1);
    if (!bindgroups.empty()) {
        auto& bg = bindgroups.at(0);
        CHECK_EQ(bg.entries.size(), 2);

        auto lut_it = std::find_if(bg.entries.begin(), bg.entries.end(), [](const auto& e) {
            return e.type == GPUResourceType::TEXTURE;
        });
        CHECK(lut_it != bg.entries.end());
        if (lut_it != bg.entries.end()) {
            CHECK_EQ(lut_it->binding.index, 7);
        }

        auto smp_it = std::find_if(bg.entries.begin(), bg.entries.end(), [](const auto& e) {
            return e.type == GPUResourceType::SAMPLER;
        });
        CHECK(smp_it != bg.entries.end());
        if (smp_it != bg.entries.end()) {
            CHECK_EQ(smp_it->binding.index, 1);
        }
    }
}

#ifdef LYRA_VULKAN_SUPPORT
TEST_CASE("slc::vulkan::shader_reflection" * doctest::description("shader vertex attributes reflection"))
{
    test_shader_vertex_attribute_reflection(
        CompileTarget::SPIRV,
        CompileFlag::DEBUG | CompileFlag::REFLECT);
}

TEST_CASE("slc::vulkan::explicit_group_reflection" * doctest::description("shader explicit group reflection"))
{
    test_shader_explicit_group_and_binding_reflection(
        CompileTarget::SPIRV,
        CompileFlag::DEBUG | CompileFlag::REFLECT);
}

TEST_CASE("slc::vulkan::explicit_two_arg_binding_reflection" * doctest::description("shader explicit two-arg binding reflection"))
{
    test_shader_explicit_two_arg_binding_reflection(
        CompileTarget::SPIRV,
        CompileFlag::DEBUG | CompileFlag::REFLECT);
}
#endif

#ifdef WIN32
TEST_CASE("slc::d3d12::shader_reflection" * doctest::description("shader vertex attributes reflection"))
{
    test_shader_vertex_attribute_reflection(
        CompileTarget::DXIL,
        CompileFlag::DEBUG | CompileFlag::REFLECT);
}

TEST_CASE("slc::d3d12::explicit_group_reflection" * doctest::description("shader explicit group reflection"))
{
    test_shader_explicit_group_and_binding_reflection(
        CompileTarget::DXIL,
        CompileFlag::DEBUG | CompileFlag::REFLECT);
}

TEST_CASE("slc::d3d12::explicit_two_arg_binding_reflection" * doctest::description("shader explicit two-arg binding reflection"))
{
    test_shader_explicit_two_arg_binding_reflection(
        CompileTarget::DXIL,
        CompileFlag::DEBUG | CompileFlag::REFLECT);
}
#endif

#ifdef __APPLE__
TEST_CASE("slc::metal::shader_reflection" * doctest::description("shader vertex attributes reflection"))
{
    test_shader_vertex_attribute_reflection(
        CompileTarget::MSL,
        CompileFlag::DEBUG | CompileFlag::REFLECT);
}

TEST_CASE("slc::metal::explicit_group_reflection" * doctest::description("shader explicit group reflection"))
{
    test_shader_explicit_group_and_binding_reflection(
        CompileTarget::MSL,
        CompileFlag::DEBUG | CompileFlag::REFLECT);
}

TEST_CASE("slc::metal::explicit_two_arg_binding_reflection" * doctest::description("shader explicit two-arg binding reflection"))
{
    test_shader_explicit_two_arg_binding_reflection(
        CompileTarget::MSL,
        CompileFlag::DEBUG | CompileFlag::REFLECT);
}
#endif

#include <Lyra/Utilities/Plugin.h>
#include <Lyra/Compiler/SLCAPI.h>

TEST_CASE("utilities::plugin::load_error_retention" * doctest::description("plugin keeps previously loaded module when load fails"))
{
    Plugin<ShaderAPI> plugin("lyra-slang");
    CHECK(plugin.is_loaded());
    CHECK(static_cast<bool>(plugin));
    CHECK(plugin.get_api()->create_compiler != nullptr);

    // Attempt to load a non-existent plugin; should return false without exiting
    bool loaded = plugin.load("non_existent_plugin_12345");
    CHECK(!loaded);

    // The previously loaded module must remain valid and intact
    CHECK(plugin.is_loaded());
    CHECK(static_cast<bool>(plugin));
    CHECK(plugin.get_api()->create_compiler != nullptr);

    // Unload the plugin; all function pointers and state must be reset
    plugin.unload();
    CHECK(!plugin.is_loaded());
    CHECK(!static_cast<bool>(plugin));
    CHECK(plugin.get_api()->create_compiler == nullptr);
}

#include <Lyra/Utilities/Detail/Blackboard.h>

TEST_CASE("utilities::blackboard::const_methods" * doctest::description("blackboard const methods and queries"))
{
    lyra::detail::Blackboard bb;
    CHECK(bb.empty());
    CHECK(bb.size() == 0);
    CHECK(!bb.has<int>());

    bb.add<int>(42);
    CHECK(!bb.empty());
    CHECK(bb.size() == 1);
    CHECK(bb.has<int>());

    // Test const access through const reference
    const lyra::detail::Blackboard& const_bb = bb;
    CHECK(!const_bb.empty());
    CHECK(const_bb.size() == 1);
    CHECK(const_bb.has<int>());
    CHECK(const_bb.get<int>() == 42);
    CHECK(const_bb.try_get<int>() != nullptr);
    CHECK(*const_bb.try_get<int>() == 42);
    CHECK(const_bb.try_get<float>() == nullptr);

    // Test remove and clear
    CHECK(bb.remove<int>());
    CHECK(bb.empty());
    CHECK(bb.size() == 0);

    bb.add<int>(10);
    bb.add<float>(3.14f);
    CHECK(bb.size() == 2);
    bb.clear();
    CHECK(bb.empty());
    CHECK(bb.size() == 0);
}

#include <Lyra/Utilities/Detail/Toolboard.h>

TEST_CASE("utilities::toolboard::operations" * doctest::description("toolboard registration, querying, and const operations"))
{
    lyra::detail::Toolboard tb;
    CHECK(tb.empty());
    CHECK(tb.size() == 0);

    struct DummyDevice
    {
        int id = 7;
    } device;

    struct DummyWindow
    {
        int width = 800;
    } window;

    // Register via pointer
    tb.add(&device);
    CHECK(!tb.empty());
    CHECK(tb.size() == 1);
    CHECK(tb.has<DummyDevice>());
    CHECK(tb.has<DummyDevice*>());

    // Register via reference
    tb.add(window);
    CHECK(tb.size() == 2);
    CHECK(tb.has<DummyWindow>());

    // Query through const reference
    const lyra::detail::Toolboard& const_tb = tb;
    CHECK(const_tb.size() == 2);
    CHECK(!const_tb.empty());
    CHECK(const_tb.get<DummyDevice*>()->id == 7);
    CHECK(const_tb.get<DummyDevice>().id == 7);
    CHECK(const_tb.try_get<DummyDevice>() != nullptr);
    CHECK(const_tb.try_get<DummyDevice>()->id == 7);

    CHECK(const_tb.get<DummyWindow>().width == 800);
    CHECK(const_tb.get<DummyWindow*>()->width == 800);

    struct DummyCompiler
    {
    };
    CHECK(!const_tb.has<DummyCompiler>());
    CHECK(const_tb.try_get<DummyCompiler>() == nullptr);

    // Non-const mutation via get()
    tb.get<DummyDevice*>()->id = 99;
    CHECK(device.id == 99);

    // Remove
    CHECK(tb.remove<DummyDevice>());
    CHECK(tb.size() == 1);
    CHECK(!tb.has<DummyDevice>());

    // Clear
    tb.clear();
    CHECK(tb.empty());
    CHECK(tb.size() == 0);
}

TEST_CASE("shader_library::modules_compilation")
{
    String code = R"""(
    import lyra;
    import Common.Sampling;
    import Common.BSDF;
    import Common.Noise;
    import Common.Tonemapping;
    import Common.Lighting;
    import Common.RayTracing;
    import Common.Culling;
    import Common.Skinning;
    import Common.Denoising;
    import Common.Colorspace;
    import Common.SphericalHarmonics;
    import Common.SDF;
    import Common.Shadows;
    import Common.Packing;
    import Common.Mapping;
    import Common.ScreenSpace;
    import Common.Camera;
    import Procedural.Atmosphere;
    import Procedural.Ocean;
    import Procedural.Terrain;

    struct TestOutput
    {
        float4 color : SV_TARGET;
    };

    [shader("fragment")]
    TestOutput fsmain(float4 pos : SV_Position)
    {
        TestOutput out;
        float2 uv = pos.xy / 800.0;
        float n_val  = perlin_noise(uv * 10.0);
        float s_val  = simplex_noise(float3(uv, 1.0));
        float f_val  = fbm(uv * 4.0, 4);
        float3 c_val = curl_noise(float3(uv, 0.5));
        float dither = interleaved_gradient_noise(pos.xy, 0);

        PBRMaterial mat;
        mat.base_color   = float3(n_val, s_val, f_val);
        mat.metallic     = 0.0;
        mat.roughness    = 0.5;
        mat.reflectance  = 0.5;
        mat.clear_coat.factor    = 0.5;
        mat.clear_coat.roughness = 0.04;
        mat.clear_coat.tint      = float3(0.0);
        mat.clear_coat.thickness = 0.0;

        float3 V = float3(0.0, 0.0, 1.0);
        float3 L = normalize(float3(1.0, 1.0, 1.0));
        float3 N = float3(0.0, 0.0, 1.0);
        float ies = evaluate_ies_analytical(1, L, float3(0.0, 0.0, -1.0));
        float3 lit = mat.evaluate(V, L, N) * ies + c_val * 0.01 + dither * 0.001;

        // test ray tracing utilities
        float3 safe_origin = offset_ray(float3(pos.xy, 0.0), N);
        float mis = power_heuristic(1.0, 2.0);
        float3 trans = beer_lambert(float3(0.1, 0.2, 0.3), 1.0);
        uint packed_n = pack_normal_2x16(N);
        float3 unpacked_n = unpack_normal_2x16(packed_n);

        // test culling utilities
        float4 planes[6] = { float4(1, 0, 0, 1), float4(-1, 0, 0, 1), float4(0, 1, 0, 1), float4(0, -1, 0, 1), float4(0, 0, 1, 1), float4(0, 0, -1, 1) };
        float3 aabb_min = float3(-1.0);
        float3 aabb_max = float3(1.0);
        bool culled = cull_aabb_frustum(planes, aabb_min, aabb_max);
        bool cone_culled = cull_cluster_cone(float3(0, 5, 0), float3(0), float3(0, 1, 0), 0.5);

        // test skinning utilities
        float4 weights = normalize_weights_4(float4(0.5, 0.3, 0.2, 0.0));
        DualQuat dq;
        dq.real = float4(0, 0, 0, 1);
        dq.dual = float4(0, 0, 0, 0);
        float3 skinned_p = dq.transform_point(float3(pos.xy, 0.0));
        float3 morph_pos = skinned_p;
        float3 morph_norm = N;
        float3 morph_tang = float3(1, 0, 0);
        apply_morph_target(morph_pos, morph_norm, morph_tang, float3(0.1), float3(0), float3(0), 0.5);

        // test denoising utilities
        float3 ycocg = rgb_to_ycocg(lit);
        float3 clipped_ycocg = clip_variance_aabb(ycocg, ycocg, float3(0.1));
        float3 denoised = ycocg_to_rgb(clipped_ycocg);
        float atrous_w = atrous_kernel_weight(0, 0);
        float edge_w = edge_stopping_weight(1.0, 1.02, N, N, 0.5, 0.52, 0.01);

        // test colorspace utilities
        float3 acescg = srgb_to_acescg(denoised);
        float3 kelvin_col = kelvin_to_rgb(6500.0);
        float3 xyz = linear_srgb_to_xyz(kelvin_col);
        float3 lab = xyz_to_lab(xyz);
        float3 p3_col = srgb_to_dci_p3(denoised);

        // test spherical harmonics utilities
        SH9Color sun_sh = project_directional_light(float3(0, 1, 0), float3(1.0, 0.95, 0.9));
        SH9Color irr_sh = convolve_cosine_lobe(sun_sh);
        SH9Color windowed_sh = apply_sloan_windowing(irr_sh, 0.05);
        float3 sh_irradiance = evaluate_sh_irradiance(windowed_sh, N);

        // test SDF utilities (Inigo Quilez)
        float sphere_d = sdf_sphere(float3(pos.xy, 0.0), 1.0);
        float box_d = sdf_box(float3(pos.xy, 0.0), float3(0.5));
        float smooth_d = op_smooth_union(sphere_d, box_d, 0.1);
        float3 twisted_p = op_twist(float3(pos.xy, 0.0), 0.5);
        SphereSDF s_scene;
        s_scene.radius = 1.0;
        float3 s_normal = sdf_calc_normal(float3(0, 1, 0), 0.001, s_scene);

        // test Shadows utilities
        float3 biased_p = apply_normal_offset_bias(float3(pos.xy, 0.0), N, L, 0.02);
        float slope_bias = compute_slope_scaled_bias(max(dot(N, L), 0.0));
        int cascade_idx;
        float cascade_blend;
        select_csm_cascade(15.0, float4(10, 30, 100, 300), 0.15, cascade_idx, cascade_blend);

        // test Packing utilities
        uint r11 = pack_float3_to_r11g11b10f(lit);
        float3 unpacked_r11 = unpack_r11g11b10f_to_float3(r11);
        uint pack_n, pack_t;
        pack_tangent_frame(N, float3(1, 0, 0), 1.0, pack_n, pack_t);
        float3 dec_n, dec_t, dec_b;
        unpack_tangent_frame(pack_n, pack_t, dec_n, dec_t, dec_b);
        float lin_z = linearize_reverse_z_depth(0.9, 0.1, 1000.0);

        // test Mapping and ScreenSpace utilities
        float3 tri_w = triplanar_weights(N, 4.0);
        float edge_att = screen_edge_fade(uv);
        float arc_ao = gtao_integrate_arc(-0.5, 0.5, 0.0);

        // test Camera & Exposure utilities
        float ev100 = camera_parameters_to_ev100(4.0, 1.0 / 125.0, 100.0);
        float exp_scale = ev100_to_exposure(ev100);

        // test 2D SDF utilities (Inigo Quilez)
        float circ_2d = sdf_circle_2d(uv, 0.5);
        float box_2d = sdf_box_2d(uv, float2(0.3, 0.2));
        float extr_d = op_extrude(float3(pos.xy, 0.0), circ_2d, 0.1);

        // test Procedural Atmosphere
        AtmosphereParameters atmo = default_earth_atmosphere();
        float3 sky_col = evaluate_sky_radiance(atmo, float3(0, 6361.0, 0), normalize(float3(0, 0.1, 1)), normalize(float3(0, 0.5, 1)), 4);

        // test Procedural Ocean
        GerstnerWave waves[4];
        for (int w_i = 0; w_i < 4; ++w_i) {
            waves[w_i].direction = float2(1, 0);
            waves[w_i].amplitude = 0.5;
            waves[w_i].wavelength = 20.0;
            waves[w_i].steepness = 0.2;
            waves[w_i].phase = 0.0;
        }
        WaterSurfacePoint water_pt = evaluate_gerstner_waves(pos.xy, 1.0, waves);
        float3 water_trans = compute_water_transmittance(5.0);
        float water_fresnel = compute_water_fresnel(0.8);

        // test Procedural Terrain
        TerrainBiomeConfig biome = default_mountain_biome();
        float elev = evaluate_ridged_elevation(pos.xy);
        float3 terr_norm = evaluate_terrain_normal(pos.xy);
        TerrainSplatWeights splat = evaluate_terrain_splat_weights(elev * 800.0, terr_norm, biome);

        out.color = float4(tonemap_aces_film(sh_irradiance * 0.1 + acescg_to_srgb(acescg) * atrous_w * edge_w * mis * trans + unpacked_n * 0.0001 + morph_pos * 0.00001 + unpacked_r11 * 0.0001 + dec_n * 0.0001 + sky_col * 0.01 + water_trans * 0.01 + splat.grass * 0.01 + (culled || cone_culled ? 0.001 : 0.0) + (smooth_d + slope_bias + lin_z + edge_att + arc_ao + exp_scale + circ_2d + box_2d + extr_d + water_fresnel) * 0.00001), 1.0);
        return out;
    }
    )""";

    auto compiler = execute([&]() {
        auto desc   = CompilerDescriptor{};
        desc.target = CompileTarget::SPIRV;
        desc.flags  = CompileFlag::NONE;
        return Compiler::init(desc);
    });

    auto module = execute([&]() {
        auto desc   = CompileDescriptor{};
        desc.module = "test_shader_library";
        desc.path   = "test_shader_library.slang";
        desc.source = code.c_str();
        return compiler->compile(desc);
    });

    CHECK(module != nullptr);
    CHECK(module->handle.valid());

    auto reflection = compiler->reflect({
        {*module, "fsmain"},
    });
    CHECK(reflection != nullptr);

    auto ies_module = execute([&]() {
        auto desc   = CompileDescriptor{};
        desc.module = "LUT.IESProfiles";
        desc.path   = "Engine/Assets/Shaders/LUT/IESProfiles.slang";
        return compiler->compile(desc);
    });
    CHECK(ies_module != nullptr);
    CHECK(ies_module->handle.valid());

    auto ies_reflection = compiler->reflect({
        {*ies_module, "csmain"},
    });
    CHECK(ies_reflection != nullptr);
    CHECK_EQ(ies_reflection->get_bind_group_layouts().size(), 1);

    auto culling_module = execute([&]() {
        auto desc   = CompileDescriptor{};
        desc.module = "GPUDriven.Culling";
        desc.path   = "Engine/Assets/Shaders/GPUDriven/Culling.slang";
        return compiler->compile(desc);
    });
    CHECK(culling_module != nullptr);
    CHECK(culling_module->handle.valid());

    auto culling_reflection = compiler->reflect({
        {*culling_module, "csmain"},
    });
    CHECK(culling_reflection != nullptr);
    CHECK_EQ(culling_reflection->get_bind_group_layouts().size(), 1);

    // post-processing shaders: AutoExposure, Bloom, DepthOfField, FXAA
    auto exposure_module = execute([&]() {
        auto desc   = CompileDescriptor{};
        desc.module = "PostProcess.AutoExposure";
        desc.path   = "Engine/Assets/Shaders/PostProcess/AutoExposure.slang";
        return compiler->compile(desc);
    });
    CHECK(exposure_module != nullptr);
    CHECK(exposure_module->handle.valid());

    auto bloom_module = execute([&]() {
        auto desc   = CompileDescriptor{};
        desc.module = "PostProcess.Bloom";
        desc.path   = "Engine/Assets/Shaders/PostProcess/Bloom.slang";
        return compiler->compile(desc);
    });
    CHECK(bloom_module != nullptr);
    CHECK(bloom_module->handle.valid());

    auto dof_module = execute([&]() {
        auto desc   = CompileDescriptor{};
        desc.module = "PostProcess.DepthOfField";
        desc.path   = "Engine/Assets/Shaders/PostProcess/DepthOfField.slang";
        return compiler->compile(desc);
    });
    CHECK(dof_module != nullptr);
    CHECK(dof_module->handle.valid());

    auto fxaa_module = execute([&]() {
        auto desc   = CompileDescriptor{};
        desc.module = "PostProcess.FXAA";
        desc.path   = "Engine/Assets/Shaders/PostProcess/FXAA.slang";
        return compiler->compile(desc);
    });
    CHECK(fxaa_module != nullptr);
    CHECK(fxaa_module->handle.valid());
}
