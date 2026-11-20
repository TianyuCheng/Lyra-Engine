#include <regex>
#include <Lyra/Graphics/RHIUtils.h>

#include "SlangPreprocessor.h"

using namespace lyra;

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

char SlangPreprocessor::get_reg_type(const String& d)
{
    if (d.find("Sampler") != String::npos) return 's';
    if (d.find("RW") != String::npos) return 'u';
    if (d.find("ConstantBuffer") != String::npos || d.find("cbuffer") != String::npos) return 'b';
    return 't';
}

String SlangPreprocessor::process_host_target(const String& input, String& wrappers)
{
    String result = input;

    static const std::regex entry_re(R"re(\[shader\("(vertex|fragment)"\)\]\s*(?:public\s+)?([A-Za-z0-9_]+)\s+([A-Za-z0-9_]+)\s*\(([^)]*)\))re");

    auto words_begin = std::sregex_iterator(result.begin(), result.end(), entry_re);
    auto words_end   = std::sregex_iterator();

    for (auto it = words_begin; it != words_end; ++it) {
        const std::smatch& m         = *it;
        String             stage     = m[1].str();
        String             ret_type  = m[2].str();
        String             func_name = m[3].str();
        String             params    = m[4].str();

        // parse input parameter type
        String                  in_type = "";
        std::smatch             pm;
        static const std::regex param_type_re(R"(\s*(?:uniform\s+)?([A-Za-z0-9_]+))");
        if (std::regex_search(params, pm, param_type_re)) {
            in_type = pm[1].str();
        }

        if (in_type.empty() || in_type == "void") {
            wrappers += "\n[shader(\"compute\")]\n[numthreads(1,1,1)]\nvoid " + func_name + "_Callable(uniform " + ret_type + "* out_v) {\n    *out_v = " + func_name + "();\n}\n";
        } else {
            wrappers += "\n[shader(\"compute\")]\n[numthreads(1,1,1)]\nvoid " + func_name + "_Callable(uniform " + in_type + "* in_v, uniform " + ret_type + "* out_v) {\n    *out_v = " + func_name + "(*in_v);\n}\n";
        }
    }

    // strip [shader("vertex")] and [shader("fragment")]
    result = std::regex_replace(result, std::regex(R"(\[shader\("vertex"\)\])"), "");
    result = std::regex_replace(result, std::regex(R"(\[shader\("fragment"\)\])"), "");

    // strip : SV_Target or : SV_TARGET
    result = std::regex_replace(result, std::regex(R"(:\s*SV_[Tt][Aa][Rr][Gg][Ee][Tt]\d?)"), "");

    return result;
}

String SlangPreprocessor::process_attributes(const String& input, CompileTarget target)
{
    String result = input;

    // 1. immediates: [[lyra::immediates]] or [lyra::immediates]
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
        } else if (target == CompileTarget::HOST) {
            return decl + ";";
        }
        return m[0].str();
    });

    // 2. two-argument binding: [[lyra::binding(binding, set)]]
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
        } else if (target == CompileTarget::HOST) {
            return decl + ";";
        }
        return m[0].str();
    });

    // 2.1 combined group and binding: [[lyra::group(set)]] [[lyra::binding(binding)]]
    static const std::regex group_binding_regex(R"((\[\[?\s*(?:lyra::)?(?:group|set)\s*\(\s*(\d+)\s*\)\s*\]?\])\s*(\[\[?\s*(?:lyra::)?binding\s*\(\s*(\d+)\s*\)\s*\]?\])\s*([^;]+);)");
    result = regex_replace_callback(result, group_binding_regex, [&](const std::smatch& m) -> String {
        String s    = m[2].str();
        String b    = m[4].str();
        String decl = m[5].str();

        char reg = get_reg_type(decl);
        if (target == CompileTarget::SPIRV) {
            if (decl.find("vk::binding") == String::npos) {
                return "[[vk::binding(" + b + ", " + s + ")]] [[lyra::group(" + s + ")]] [[lyra::binding(" + b + ")]] " + decl + ";";
            }
        } else if (target == CompileTarget::DXIL || target == CompileTarget::MSL) {
            if (decl.find("register") == String::npos) {
                return "[[lyra::group(" + s + ")]] [[lyra::binding(" + b + ")]] " + decl + " : register(" + reg + b + ", space" + s + ");";
            }
        } else if (target == CompileTarget::HOST) {
            return decl + ";";
        }
        return m[0].str();
    });

    // 2.2 combined binding and group: [[lyra::binding(binding)]] [[lyra::group(set)]]
    static const std::regex binding_group_regex(R"((\[\[?\s*(?:lyra::)?binding\s*\(\s*(\d+)\s*\)\s*\]?\])\s*(\[\[?\s*(?:lyra::)?(?:group|set)\s*\(\s*(\d+)\s*\)\s*\]?\])\s*([^;]+);)");
    result = regex_replace_callback(result, binding_group_regex, [&](const std::smatch& m) -> String {
        String b    = m[2].str();
        String s    = m[4].str();
        String decl = m[5].str();

        char reg = get_reg_type(decl);
        if (target == CompileTarget::SPIRV) {
            if (decl.find("vk::binding") == String::npos) {
                return "[[vk::binding(" + b + ", " + s + ")]] [[lyra::group(" + s + ")]] [[lyra::binding(" + b + ")]] " + decl + ";";
            }
        } else if (target == CompileTarget::DXIL || target == CompileTarget::MSL) {
            if (decl.find("register") == String::npos) {
                return "[[lyra::group(" + s + ")]] [[lyra::binding(" + b + ")]] " + decl + " : register(" + reg + b + ", space" + s + ");";
            }
        } else if (target == CompileTarget::HOST) {
            return decl + ";";
        }
        return m[0].str();
    });

    // 3. group / set: [[lyra::group(N)]] or [[lyra::set(N)]]
    static const std::regex group_regex(R"((\[\[?\s*(?:lyra::)?(?:group|set)\s*\(\s*(\d+)\s*\)\s*\]?\])\s*([^;]+);)");
    result = regex_replace_callback(result, group_regex, [&](const std::smatch& m) -> String {
        String attr = m[1].str();
        String num  = m[2].str();
        String decl = m[3].str();

        char reg            = get_reg_type(decl);
        bool is_param_block = (decl.find("ParameterBlock") != String::npos);

        if (target == CompileTarget::SPIRV) {
            if (decl.find("vk::binding") == String::npos) {
                return "[[vk::binding(0, " + num + ")]] " + attr + " " + decl + ";";
            }
        } else if (target == CompileTarget::DXIL) {
            if (decl.find("register") == String::npos) {
                if (is_param_block) {
                    return attr + " " + decl + " : register(space" + num + ");";
                }
                return attr + " " + decl + " : register(" + reg + "0, space" + num + ");";
            }
        } else if (target == CompileTarget::MSL) {
            if (decl.find("register") == String::npos) {
                if (is_param_block) {
                    return attr + " " + decl + " : register(b" + num + ", space" + num + ");";
                }
                return attr + " " + decl + " : register(" + reg + "0, space" + num + ");";
            }
        } else if (target == CompileTarget::HOST) {
            return decl + ";";
        }
        return m[0].str();
    });

    // 4. single-argument binding: [[lyra::binding(N)]]
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
        } else if (target == CompileTarget::HOST) {
            return decl + ";";
        }
        return m[0].str();
    });

    return result;
}

String SlangPreprocessor::process(const String& input, CompileTarget target)
{
    String result = input;

    if (target == CompileTarget::HOST) {
        String wrappers;
        result = process_host_target(result, wrappers);
        result += wrappers;
    }

    // workaround for Slang upstream bug #12165:
    // vector fwidth capability annotation is missing for Metal target
    if (target == CompileTarget::MSL) {
        result = "#define fwidth(x) (abs(ddx(x)) + abs(ddy(x)))\n" + result;
    }

    return process_attributes(result, target);
}
