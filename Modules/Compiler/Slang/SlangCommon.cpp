#include <cassert>
#include "SlangCommon.h"
#include "SlangPreprocessor.h"

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

String preprocess_lyra_shader_source(const String& input, CompileTarget target)
{
    return SlangPreprocessor::process(input, target);
}
