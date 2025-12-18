#include "helper.h"

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
    ParameterBlock<Hello> bibi;

    [shader("vertex")]
    VertexOutput vsmain(VertexInput input)
    {
        VertexOutput output;
        output.color = input.color;
        output.position = float4(input.position, 1.0);
        output.position = mul(output.position, haha.cam.view);
        output.position = mul(output.position, bibi.cam.proj);
        return output;
    }

    [shader("fragment")]
    float4 fsmain(VertexOutput input) : SV_TARGET
    {
        return haha.tex.Sample(haha.smp, input.color.xy);
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

    auto attributes = reflection->get_vertex_attributes({
        {"position", offsetof(Vertex, position)},
        {"texcoord", offsetof(Vertex, uv)},
    });

    std::cout << "attributes.size() = " << attributes.size() << std::endl;
    for (auto& attrib : attributes) {
        std::cout << "- attrib[" << attrib.shader_location << "].offset = " << attrib.offset << std::endl;
        std::cout << "- attrib[" << attrib.shader_location << "].location = " << attrib.shader_location << std::endl;
        std::cout << "- attrib[" << attrib.shader_location << "].semantic = " << attrib.shader_semantic << std::endl;
    }

    auto bindgroups = reflection->get_bind_group_layouts();
    std::cout << "bindgroups.size() = " << bindgroups.size() << std::endl;
    for (auto& bindgroup : bindgroups) {
        std::cout << "bindgroup: " << (bindgroup.label ? bindgroup.label : "unnamed") << std::endl;
        for (auto& entry : bindgroup.entries) {
            std::cout << "- entries[" << entry.binding.index << "].type       = " << (int)entry.type << std::endl;
            std::cout << "- entries[" << entry.binding.index << "].binding    = " << entry.binding.index << std::endl;
            std::cout << "- entries[" << entry.binding.index << "].count      = " << entry.count << std::endl;
            std::cout << "- entries[" << entry.binding.index << "].visibility =";
            if (entry.visibility.contains(GPUShaderStage::VERTEX)) std::cout << " VERTEX";
            if (entry.visibility.contains(GPUShaderStage::FRAGMENT)) std::cout << " FRAGMENT";
            if (entry.visibility.contains(GPUShaderStage::COMPUTE)) std::cout << " COMPUTE";
            std::cout << std::endl;
        }
    }

    auto push_constants = reflection->get_push_constant_ranges();
    std::cout << "push_contants.size() = " << push_constants.size() << std::endl;
    for (auto& push_constant : push_constants) {
        std::cout << "- push_constant.offset = " << push_constant.offset << std::endl;
        std::cout << "- push_constant.size   = " << push_constant.size << std::endl;
        std::cout << "- push_constant.visibility = " << std::endl;
        if (push_constant.visibility.contains(GPUShaderStage::VERTEX)) std::cout << " VERTEX";
        if (push_constant.visibility.contains(GPUShaderStage::FRAGMENT)) std::cout << " FRAGMENT";
        if (push_constant.visibility.contains(GPUShaderStage::COMPUTE)) std::cout << " COMPUTE";
    }
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
