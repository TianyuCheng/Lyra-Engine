#include "SlangCommon.h"
#include <regex>
#include <cassert>

using namespace lyra;

static ComPtr<slang::IGlobalSession> GLOBAL_SESSION;
static Logger                        logger = create_logger("Slang", LogLevel::trace);

Logger get_logger()
{
    return logger;
}

ComPtr<slang::IGlobalSession>& get_global_session()
{
    return GLOBAL_SESSION;
}

void diagnose_if_needed(slang::IBlob* diagnosticsBlob, SlangResult result)
{
    if (diagnosticsBlob != nullptr) {
        auto msg = static_cast<const char*>(diagnosticsBlob->getBufferPointer());
        if (SLANG_FAILED(result))
            get_logger()->error("Slang diagnostics: {}", msg);
        else
            get_logger()->warn("Slang diagnostics: {}", msg);
    }
}

String to_string(GPUShaderStageFlags flags)
{
    String result = "";
    // clang-format off
    if (flags.contains(GPUShaderStage::COMPUTE))  result += "compute ";
    if (flags.contains(GPUShaderStage::VERTEX))   result += "vertex ";
    if (flags.contains(GPUShaderStage::FRAGMENT)) result += "fragment ";
    // clang-format on
    return result;
}

CString to_string(slang::TypeReflection::Kind kind)
{
    // clang-format off
    switch (kind) {
        case slang::TypeReflection::Kind::None:                 return "none";
        case slang::TypeReflection::Kind::Scalar:               return "scalar";
        case slang::TypeReflection::Kind::Vector:               return "vector";
        case slang::TypeReflection::Kind::Matrix:               return "matrix";
        case slang::TypeReflection::Kind::Array:                return "array";
        case slang::TypeReflection::Kind::Struct:               return "struct";
        case slang::TypeReflection::Kind::Resource:             return "resource";
        case slang::TypeReflection::Kind::SamplerState:         return "sampler_state";
        case slang::TypeReflection::Kind::TextureBuffer:        return "texture_buffer";
        case slang::TypeReflection::Kind::ShaderStorageBuffer:  return "shader_storage_buffer";
        case slang::TypeReflection::Kind::ParameterBlock:       return "parameter_block";
        case slang::TypeReflection::Kind::GenericTypeParameter: return "generic_type_parameter";
        case slang::TypeReflection::Kind::Interface:            return "interface";
        case slang::TypeReflection::Kind::OutputStream:         return "output_stream";
        case slang::TypeReflection::Kind::Specialized:          return "specialized";
        case slang::TypeReflection::Kind::Feedback:             return "feedback";
        case slang::TypeReflection::Kind::Pointer:              return "pointer";
        case slang::TypeReflection::Kind::ConstantBuffer:       return "constant_buffer";
        default:                                                return "unknown";
    }
    // clang-format on
}

CString to_string(slang::ParameterCategory category)
{
    // clang-format off
    switch (category) {
        case slang::ParameterCategory::None:                    return "none";
        case slang::ParameterCategory::Mixed:                   return "mixed";
        case slang::ParameterCategory::ConstantBuffer:          return "constant_buffer";
        case slang::ParameterCategory::ShaderResource:          return "shader_resource";
        case slang::ParameterCategory::UnorderedAccess:         return "unordered_access";
        case slang::ParameterCategory::VaryingInput:            return "varying_input";
        case slang::ParameterCategory::VaryingOutput:           return "varying_output";
        case slang::ParameterCategory::SamplerState:            return "sampler_state";
        case slang::ParameterCategory::Uniform:                 return "uniform";
        case slang::ParameterCategory::DescriptorTableSlot:     return "descriptor_table_slot";
        case slang::ParameterCategory::SpecializationConstant:  return "specialization_constant";
        case slang::ParameterCategory::PushConstantBuffer:      return "push_constant_buffer";
        case slang::ParameterCategory::RegisterSpace:           return "register_space";
        case slang::ParameterCategory::GenericResource:         return "generic_resource";
        case slang::ParameterCategory::RayPayload:              return "ray_payload";
        case slang::ParameterCategory::HitAttributes:           return "hit_attributes";
        case slang::ParameterCategory::CallablePayload:         return "callable_payload";
        case slang::ParameterCategory::ShaderRecord:            return "shader_record";
        case slang::ParameterCategory::ExistentialTypeParam:    return "existential_type_param";
        case slang::ParameterCategory::ExistentialObjectParam:  return "existential_object_param";
        case slang::ParameterCategory::MetalPayload:            return "metal_payload";
        case slang::ParameterCategory::MetalAttribute:          return "metal_attribute";
        case slang::ParameterCategory::SubElementRegisterSpace: return "subelement_register_space";
        default:                                                return "unknown";
    }
    // clang-format on
}

GPUShaderStage to_stage(SlangStage stage)
{
    // clang-format off
    switch (stage) {
        case SLANG_STAGE_VERTEX:         return GPUShaderStage::VERTEX;
        case SLANG_STAGE_FRAGMENT:       return GPUShaderStage::FRAGMENT;
        case SLANG_STAGE_COMPUTE:        return GPUShaderStage::COMPUTE;
        case SLANG_STAGE_RAY_GENERATION: return GPUShaderStage::RAYGEN;
        case SLANG_STAGE_INTERSECTION:   return GPUShaderStage::INTERSECT;
        case SLANG_STAGE_ANY_HIT:        return GPUShaderStage::AHIT;
        case SLANG_STAGE_CLOSEST_HIT:    return GPUShaderStage::CHIT;
        case SLANG_STAGE_MISS:           return GPUShaderStage::MISS;
        case SLANG_STAGE_GEOMETRY:
        case SLANG_STAGE_HULL:
        case SLANG_STAGE_DOMAIN:
        case SLANG_STAGE_AMPLIFICATION:
        case SLANG_STAGE_MESH:
        case SLANG_STAGE_CALLABLE:
        default:
            assert(!"Unsupported shader stage!");
            return GPUShaderStage::COMPUTE;
    }
    // clang-format on
}

template <typename Callback>
static String regex_replace_callback(const String& input, const std::regex& re, Callback&& callback)
{
    String result;
    auto   words_begin = std::sregex_iterator(input.begin(), input.end(), re);
    auto   words_end   = std::sregex_iterator();

    usize last_pos = 0;
    for (auto it = words_begin; it != words_end; ++it) {
        const std::smatch& match = *it;
        result.append(input, last_pos, match.position() - last_pos);
        result.append(callback(match));
        last_pos = match.position() + match.length();
    }
    result.append(input, last_pos, input.length() - last_pos);
    return result;
}

String preprocess_lyra_shader_source(const String& input, CompileTarget target)
{
    String result = input;

    // workaround for Slang upstream bug #12165:
    // vector fwidth capability annotation is missing for Metal target
    if (target == CompileTarget::MSL) {
        result = "#define fwidth(x) (abs(ddx(x)) + abs(ddy(x)))\n" + result;
    }

    auto get_reg_type = [](const String& d) -> char {
        if (d.find("Sampler") != String::npos) return 's';
        if (d.find("RW") != String::npos) return 'u';
        if (d.find("ConstantBuffer") != String::npos || d.find("cbuffer") != String::npos) return 'b';
        return 't';
    };

    // 1. Immediates: [[lyra::immediates]] or [lyra::immediates]
    static const std::regex imm_regex(R"((\[\[?\s*(?:lyra::)?immediates\s*\]?\])\s*([^;]+);)");
    result = regex_replace_callback(result, imm_regex, [&](const std::smatch& m) -> String {
        String attr = m[1].str();
        String decl = m[2].str();

        if (target == CompileTarget::SPIRV) {
            String prefix = "";
            if (decl.find("vk::push_constant") == String::npos && attr.find("vk::push_constant") == String::npos) {
                prefix = "[[vk::push_constant]] ";
            }
            String suffix = "";
            if (decl.find("register") == String::npos && decl.find("IMMEDIATES") == String::npos) {
                suffix = " : register(b0, space" + std::to_string(D3D12_ImmediateRegisterSpace) + ")";
            }
            return prefix + attr + " " + decl + suffix + ";";
        } else if (target == CompileTarget::DXIL) {
            if (decl.find("register") == String::npos && decl.find("IMMEDIATES") == String::npos) {
                return attr + " " + decl + " : register(b0, space" + std::to_string(D3D12_ImmediateRegisterSpace) + ");";
            }
        } else if (target == CompileTarget::MSL) {
            String prefix = "";
            if (decl.find("vk::push_constant") == String::npos && attr.find("vk::push_constant") == String::npos) {
                prefix = "[[vk::push_constant]] ";
            }
            if (decl.find("register") == String::npos && decl.find("IMMEDIATES") == String::npos) {
                return prefix + attr + " " + decl + " : register(b" + std::to_string(METAL_ImmediateBufferIndex) + ", space" + std::to_string(METAL_ImmediateBufferIndex) + ");";
            }
        }
        return m[0].str();
    });

    // 2. Two-argument binding: [[lyra::binding(binding, set)]]
    static const std::regex binding_set_regex(R"((\[\[?\s*(?:lyra::)?binding\s*\(\s*(\d+)\s*,\s*(\d+)\s*\)\s*\]?\])\s*([^;]+);)");
    result = regex_replace_callback(result, binding_set_regex, [&](const std::smatch& m) -> String {
        String attr = m[1].str();
        String b    = m[2].str();
        String s    = m[3].str();
        String decl = m[4].str();

        char reg = get_reg_type(decl);
        if (target == CompileTarget::SPIRV) {
            if (decl.find("vk::binding") == String::npos) {
                return "[[vk::binding(" + b + ", " + s + ")]] [[lyra::group(" + s + ")]] [[lyra::binding(" + b + ")]] " + decl + ";";
            }
        } else if (target == CompileTarget::DXIL || target == CompileTarget::MSL) {
            if (decl.find("register") == String::npos) {
                return "[[lyra::group(" + s + ")]] [[lyra::binding(" + b + ")]] " + decl + " : register(" + reg + b + ", space" + s + ");";
            }
        }
        return m[0].str();
    });

    // 3. Group / Set: [[lyra::group(N)]] or [[lyra::set(N)]]
    static const std::regex group_regex(R"((\[\[?\s*(?:lyra::)?(?:group|set)\s*\(\s*(\d+)\s*\)\s*\]?\])\s*([^;]+);)");
    result = regex_replace_callback(result, group_regex, [&](const std::smatch& m) -> String {
        String attr = m[1].str();
        String num  = m[2].str();
        String decl = m[3].str();

        if (target == CompileTarget::SPIRV) {
            if (decl.find("vk::binding") == String::npos) {
                return "[[vk::binding(0, " + num + ")]] " + attr + " " + decl + ";";
            }
        } else if (target == CompileTarget::DXIL) {
            if (decl.find("register") == String::npos) {
                return attr + " " + decl + " : register(space" + num + ");";
            }
        } else if (target == CompileTarget::MSL) {
            if (decl.find("register") == String::npos) {
                return attr + " " + decl + " : register(b" + num + ", space" + num + ");";
            }
        }
        return m[0].str();
    });

    // 4. Single-argument Binding: [[lyra::binding(N)]]
    static const std::regex binding_regex(R"((\[\[?\s*(?:lyra::)?binding\s*\(\s*(\d+)\s*\)\s*\]?\])\s*([^;]+);)");
    result = regex_replace_callback(result, binding_regex, [&](const std::smatch& m) -> String {
        String attr = m[1].str();
        String num  = m[2].str();
        String decl = m[3].str();

        char reg = get_reg_type(decl);
        if (target == CompileTarget::SPIRV) {
            if (decl.find("vk::binding") == String::npos) {
                return "[[vk::binding(" + num + ")]] " + attr + " " + decl + ";";
            }
        } else if (target == CompileTarget::DXIL || target == CompileTarget::MSL) {
            if (decl.find("register") == String::npos) {
                return attr + " " + decl + " : register(" + reg + num + ");";
            }
        }
        return m[0].str();
    });

    return result;
}
