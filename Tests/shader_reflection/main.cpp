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

    [[lyra::push_constant]]
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
    auto pos_attrib_it = std::find_if(attributes.begin(), attributes.end(), [](const auto& a) { return strcmp(a.shader_semantic, "POSITION") == 0; });
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
    auto tex_attrib_it = std::find_if(attributes.begin(), attributes.end(), [](const auto& a) { return strcmp(a.shader_semantic, "TEXCOORD") == 0; });
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
    auto haha_bindgroup_it = std::find_if(bindgroups.begin(), bindgroups.end(), [](const auto& a) { return strcmp(a.label, "haha") == 0; });
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
    auto hihi_bindgroup_it = std::find_if(bindgroups.begin(), bindgroups.end(), [](const auto& a) { return strcmp(a.label, "hihi") == 0; });
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

    auto push_constants = reflection->get_push_constant_ranges();
    CHECK_EQ(push_constants.size(), 2);

    // TODO: We are currently populating the push constants at ParameterBlock level. This is not correct.
    // if (push_constants.size() == 2) {
    //     // data
    //     CHECK_EQ(push_constants.at(0).offset, 0);
    //     CHECK_EQ(push_constants.at(0).size, 12);
    //     std::cerr << std::showbase << std::hex << push_constants.at(0).visibility.value << std::endl;
    //     CHECK(!push_constants.at(0).visibility.contains(GPUShaderStage::VERTEX));
    //     CHECK(!push_constants.at(0).visibility.contains(GPUShaderStage::FRAGMENT));
    //
    //     // mvp
    //     CHECK_EQ(push_constants.at(1).offset, 16);
    //     CHECK_EQ(push_constants.at(1).size, 64);
    //     std::cerr << push_constants.at(1).visibility.value << std::endl;
    //     CHECK(push_constants.at(1).visibility.contains(GPUShaderStage::VERTEX));
    //     CHECK(!push_constants.at(1).visibility.contains(GPUShaderStage::FRAGMENT));
    // }
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

    [[lyra::push_constant]]
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

    auto push_constants = reflection->get_push_constant_ranges();
    CHECK_EQ(push_constants.size(), 1);
    CHECK_EQ(push_constants.at(0).offset, 0);
    CHECK_EQ(push_constants.at(0).size, 64);
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

    struct DummyDevice {
        int id = 7;
    } device;

    struct DummyWindow {
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

    struct DummyCompiler {};
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


