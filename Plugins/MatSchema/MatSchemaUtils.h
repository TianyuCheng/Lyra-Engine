#pragma once

#include <absl/strings/ascii.h>
#include <Lyra/Common/Macros.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Enums.h>
#include <Lyra/Format/MaterialSchema.h>

namespace lyra::matschema
{

    // helper to map string to materialsemantic
    inline MaterialSemantic string_to_semantic(const String& str)
    {
        return magic_enum::enum_cast<MaterialSemantic>(str, magic_enum::case_insensitive).value_or(MaterialSemantic::NONE);
    }

    // helper to map string to materialparametertype
    inline MaterialParameterType string_to_param_type(const String& str)
    {
        return magic_enum::enum_cast<MaterialParameterType>(str, magic_enum::case_insensitive).value_or(MaterialParameterType::FLOAT);
    }

    // helper to map string to materialblendmode
    inline MaterialBlendMode string_to_blend_mode(const String& str)
    {
        return magic_enum::enum_cast<MaterialBlendMode>(str, magic_enum::case_insensitive).value_or(MaterialBlendMode::OPAQUE);
    }

    // helper to map string to gpucullmode
    inline GPUCullMode string_to_cull_mode(const String& str)
    {
        return magic_enum::enum_cast<GPUCullMode>(str, magic_enum::case_insensitive).value_or(GPUCullMode::BACK);
    }

    inline MaterialGraphValueType to_graph_value_type(const String& str)
    {
        return magic_enum::enum_cast<MaterialGraphValueType>(str, magic_enum::case_insensitive).value_or(MaterialGraphValueType::NONE);
    }

    inline String to_string(MaterialGraphValueType type)
    {
        return absl::AsciiStrToLower(magic_enum::enum_name(type));
    }

    inline MaterialGraphProperty parse_graph_property(const JSON& json)
    {
        if (json.is_boolean()) return MaterialGraphProperty(json.get<bool>());
        if (json.is_number_integer()) return MaterialGraphProperty(json.get<int>());
        if (json.is_number_unsigned()) return MaterialGraphProperty(json.get<uint>());
        if (json.is_number_float()) return MaterialGraphProperty(json.get<float>());
        if (json.is_string()) return MaterialGraphProperty(json.get<String>());
        if (json.is_array()) {
            if (json.size() == 2) return MaterialGraphProperty(Vector2(json[0], json[1]));
            if (json.size() == 3) return MaterialGraphProperty(Vector3(json[0], json[1], json[2]));
            if (json.size() == 4) return MaterialGraphProperty(Vector4(json[0], json[1], json[2], json[3]));
        }
        return MaterialGraphProperty();
    }

    inline JSON to_json(const MaterialGraphProperty& prop)
    {
        return std::visit([](auto&& arg) -> JSON {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, bool> || std::is_same_v<T, int> ||
                          std::is_same_v<T, uint> || std::is_same_v<T, float> ||
                          std::is_same_v<T, String>) {
                return arg;
            } else if constexpr (std::is_same_v<T, Vector2>) {
                return JSON::array({arg.x, arg.y});
            } else if constexpr (std::is_same_v<T, Vector3>) {
                return JSON::array({arg.x, arg.y, arg.z});
            } else if constexpr (std::is_same_v<T, Vector4>) {
                return JSON::array({arg.x, arg.y, arg.z, arg.w});
            }
            return JSON();
        }, prop.value);
    }

} // namespace lyra::matschema
