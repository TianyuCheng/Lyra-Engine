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

    [[vk::push_constant]]
    ConstantBuffer<Xform> xform1 : PUSH_CONSTANT;

    ParameterBlock<Hello> haha;
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
        CHECK_EQ(haha_bindgroup_it->entries.at(0).binding.index, 0);
        CHECK_EQ(haha_bindgroup_it->entries.at(0).count, 1);
        if (target != CompileTarget::MSL) {
            CHECK(haha_bindgroup_it->entries.at(0).visibility.contains(GPUShaderStage::VERTEX));
            CHECK(!haha_bindgroup_it->entries.at(0).visibility.contains(GPUShaderStage::FRAGMENT));
        }

        // tex (used in fragment)
        CHECK_EQ(haha_bindgroup_it->entries.at(1).type, GPUResourceType::TEXTURE);
        CHECK_EQ(haha_bindgroup_it->entries.at(1).binding.index, 1);
        CHECK_EQ(haha_bindgroup_it->entries.at(1).count, 1);
        if (target != CompileTarget::MSL) {
            CHECK(!haha_bindgroup_it->entries.at(1).visibility.contains(GPUShaderStage::VERTEX));
            CHECK(haha_bindgroup_it->entries.at(1).visibility.contains(GPUShaderStage::FRAGMENT));
        }

        // tex2 (not used)
        CHECK_EQ(haha_bindgroup_it->entries.at(2).type, GPUResourceType::TEXTURE);
        CHECK_EQ(haha_bindgroup_it->entries.at(2).binding.index, 2);
        CHECK_EQ(haha_bindgroup_it->entries.at(2).count, 1);
        if (target != CompileTarget::MSL) {
            CHECK(!haha_bindgroup_it->entries.at(2).visibility.contains(GPUShaderStage::VERTEX));
            CHECK(!haha_bindgroup_it->entries.at(2).visibility.contains(GPUShaderStage::FRAGMENT));
        }

        // smp (used in fragment)
        CHECK_EQ(haha_bindgroup_it->entries.at(3).type, GPUResourceType::SAMPLER);
        CHECK_EQ(haha_bindgroup_it->entries.at(3).binding.index, 3);
        CHECK_EQ(haha_bindgroup_it->entries.at(3).count, 1);
        if (target != CompileTarget::MSL) {
            CHECK(!haha_bindgroup_it->entries.at(3).visibility.contains(GPUShaderStage::VERTEX));
            CHECK(haha_bindgroup_it->entries.at(3).visibility.contains(GPUShaderStage::FRAGMENT));
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
        CHECK_EQ(hihi_bindgroup_it->entries.at(0).binding.index, 0);
        CHECK_EQ(hihi_bindgroup_it->entries.at(0).count, 1);
        if (target != CompileTarget::MSL) {
            CHECK(hihi_bindgroup_it->entries.at(0).visibility.contains(GPUShaderStage::VERTEX));
            CHECK(!hihi_bindgroup_it->entries.at(0).visibility.contains(GPUShaderStage::FRAGMENT));
        }

        // tex (used in fragment)
        CHECK_EQ(hihi_bindgroup_it->entries.at(1).type, GPUResourceType::TEXTURE);
        CHECK_EQ(hihi_bindgroup_it->entries.at(1).binding.index, 1);
        CHECK_EQ(hihi_bindgroup_it->entries.at(1).count, 1);
        if (target != CompileTarget::MSL) {
            CHECK(!hihi_bindgroup_it->entries.at(1).visibility.contains(GPUShaderStage::VERTEX));
            CHECK(hihi_bindgroup_it->entries.at(1).visibility.contains(GPUShaderStage::FRAGMENT));
        }

        // tex2 (not used)
        CHECK_EQ(haha_bindgroup_it->entries.at(2).type, GPUResourceType::TEXTURE);
        CHECK_EQ(haha_bindgroup_it->entries.at(2).binding.index, 2);
        CHECK_EQ(haha_bindgroup_it->entries.at(2).count, 1);
        if (target != CompileTarget::MSL) {
            CHECK(!haha_bindgroup_it->entries.at(2).visibility.contains(GPUShaderStage::VERTEX));
            CHECK(!haha_bindgroup_it->entries.at(2).visibility.contains(GPUShaderStage::FRAGMENT));
        }

        // smp (not used)
        CHECK_EQ(hihi_bindgroup_it->entries.at(3).type, GPUResourceType::SAMPLER);
        CHECK_EQ(hihi_bindgroup_it->entries.at(3).binding.index, 3);
        CHECK_EQ(hihi_bindgroup_it->entries.at(3).count, 1);
        if (target != CompileTarget::MSL) {
            CHECK(!hihi_bindgroup_it->entries.at(3).visibility.contains(GPUShaderStage::VERTEX));
            CHECK(!hihi_bindgroup_it->entries.at(3).visibility.contains(GPUShaderStage::FRAGMENT));
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

TEST_CASE("slc::vulkan::shader_reflection" * doctest::description("shader vertex attributes reflection"))
{
    test_shader_vertex_attribute_reflection(
        CompileTarget::SPIRV,
        CompileFlag::DEBUG | CompileFlag::REFLECT);
}

#ifdef WIN32
TEST_CASE("slc::d3d12::shader_reflection" * doctest::description("shader vertex attributes reflection"))
{
    test_shader_vertex_attribute_reflection(
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
#endif
