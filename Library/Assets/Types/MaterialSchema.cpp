#include <Lyra/FileIO/VFSAPI.h>
#include <Lyra/Assets/AMSServer.h>
#include <Lyra/Assets/Types/MaterialSchema.h>

using namespace lyra;

// Helper to map string to MaterialSemantic
static MaterialSemantic string_to_semantic(const String& str)
{
    // clang-format off
    if (str == "albedo")       return MaterialSemantic::ALBEDO;
    if (str == "normal")       return MaterialSemantic::NORMAL;
    if (str == "metallic")     return MaterialSemantic::METALLIC;
    if (str == "roughness")    return MaterialSemantic::ROUGHNESS;
    if (str == "occlusion")    return MaterialSemantic::OCCLUSION;
    if (str == "emissive")     return MaterialSemantic::EMISSIVE;
    if (str == "specular")     return MaterialSemantic::SPECULAR;
    if (str == "opacity")      return MaterialSemantic::OPACITY;
    if (str == "displacement") return MaterialSemantic::DISPLACEMENT;
    if (str == "custom0")      return MaterialSemantic::CUSTOM0;
    if (str == "custom1")      return MaterialSemantic::CUSTOM1;
    if (str == "custom2")      return MaterialSemantic::CUSTOM2;
    if (str == "custom3")      return MaterialSemantic::CUSTOM3;
    if (str == "custom4")      return MaterialSemantic::CUSTOM4;
    if (str == "custom5")      return MaterialSemantic::CUSTOM5;
    if (str == "custom6")      return MaterialSemantic::CUSTOM6;
    if (str == "custom7")      return MaterialSemantic::CUSTOM7;
    // clang-format on
    return MaterialSemantic::NONE;
}

// Helper to map string to MaterialParameterType
static MaterialParameterType string_to_param_type(const String& str)
{
    // clang-format off
    if (str == "uint")         return MaterialParameterType::UINT;
    if (str == "uint2")        return MaterialParameterType::UINT2;
    if (str == "uint3")        return MaterialParameterType::UINT3;
    if (str == "uint4")        return MaterialParameterType::UINT4;
    if (str == "float")        return MaterialParameterType::FLOAT;
    if (str == "float2")       return MaterialParameterType::FLOAT2;
    if (str == "float3")       return MaterialParameterType::FLOAT3;
    if (str == "float4")       return MaterialParameterType::FLOAT4;
    if (str == "texture2d")    return MaterialParameterType::TEXTURE2D;
    if (str == "texture_cube") return MaterialParameterType::TEXTURE_CUBE;
    // clang-format on
    return MaterialParameterType::FLOAT;
}

// Helper to map string to MaterialBlendMode
static MaterialBlendMode string_to_blend_mode(const String& str)
{
    // clang-format off
    if (str == "mask")  return MaterialBlendMode::MASK;
    if (str == "blend") return MaterialBlendMode::BLEND;
    // clang-format on
    return MaterialBlendMode::OPAQUE;
}

// Helper to map string to GPUCullMode
static GPUCullMode string_to_cull_mode(const String& str)
{
    // clang-format off
    if (str == "none")  return GPUCullMode::NONE;
    if (str == "front") return GPUCullMode::FRONT;
    // clang-format on
    return GPUCullMode::BACK;
}

static MaterialGraphValueType string_to_graph_value_type(const String& str)
{
    // clang-format off
    if (str == "bool")         return MaterialGraphValueType::BOOL;
    if (str == "int")          return MaterialGraphValueType::INT;
    if (str == "uint")         return MaterialGraphValueType::UINT;
    if (str == "float")        return MaterialGraphValueType::FLOAT;
    if (str == "float2")       return MaterialGraphValueType::FLOAT2;
    if (str == "float3")       return MaterialGraphValueType::FLOAT3;
    if (str == "float4")       return MaterialGraphValueType::FLOAT4;
    if (str == "texture2d")    return MaterialGraphValueType::TEXTURE2D;
    if (str == "texture_cube") return MaterialGraphValueType::TEXTURE_CUBE;
    if (str == "sampler")      return MaterialGraphValueType::SAMPLER;
    // clang-format on
    return MaterialGraphValueType::NONE;
}

static MaterialGraphProperty parse_graph_property(const JSON& json)
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

static MaterialGraphPin parse_graph_pin(const JSON& json)
{
    MaterialGraphPin pin;
    pin.id   = json["id"].get<uint>();
    pin.name = json["name"].get<String>();
    pin.type = string_to_graph_value_type(json["type"].get<String>());
    if (json.contains("default_value")) {
        pin.default_value = parse_graph_property(json["default_value"]);
    }
    return pin;
}

static void load_material_graph(const JSON& json, MaterialGraph& graph)
{
    if (json.contains("nodes") && json["nodes"].is_array()) {
        for (auto& n_json : json["nodes"]) {
            MaterialGraphNode node;
            node.id   = n_json["id"].get<uint>();
            node.type = n_json["type"].get<String>();

            if (n_json.contains("inputs") && n_json["inputs"].is_array()) {
                for (auto& p_json : n_json["inputs"]) {
                    node.inputs.push_back(parse_graph_pin(p_json));
                }
            }

            if (n_json.contains("outputs") && n_json["outputs"].is_array()) {
                for (auto& p_json : n_json["outputs"]) {
                    node.outputs.push_back(parse_graph_pin(p_json));
                }
            }

            if (n_json.contains("properties") && n_json["properties"].is_object()) {
                for (auto& [key, val] : n_json["properties"].items()) {
                    node.properties[key] = parse_graph_property(val);
                }
            }

            graph.nodes.push_back(node);
        }
    }

    if (json.contains("links") && json["links"].is_array()) {
        for (auto& l_json : json["links"]) {
            MaterialGraphLink link;
            link.id        = l_json["id"].get<uint>();
            link.from_node = l_json["from_node"].get<uint>();
            link.from_pin  = l_json["from_pin"].get<uint>();
            link.to_node   = l_json["to_node"].get<uint>();
            link.to_pin    = l_json["to_pin"].get<uint>();
            graph.links.push_back(link);
        }
    }
}

static void* load_material_schema(FileLoader* loader, FSPath path)
{
    auto content = loader->read<char>(path);
    auto json    = JSON::parse(content.begin(), content.end());

    auto schema = new MaterialSchema();

    if (json.contains("schema_id"))
        schema->schema_id = json["schema_id"].get<uint>();

    if (json.contains("display_name"))
        schema->display_name = json["display_name"].get<String>();

    if (json.contains("parameters") && json["parameters"].is_array()) {
        for (auto& p_json : json["parameters"]) {
            MaterialParameter p;
            p.name = p_json["name"].get<String>();
            p.type = string_to_param_type(p_json["type"].get<String>());

            if (p_json.contains("semantic")) {
                p.semantic = string_to_semantic(p_json["semantic"].get<String>());
            }

            if (p_json.contains("default_value")) {
                auto& val = p_json["default_value"];
                if (val.is_array() && val.size() == 4) {
                    p.default_value = Vector4(val[0], val[1], val[2], val[3]);
                } else if (val.is_number()) {
                    p.default_value = Vector4(val.get<float>(), 0.0f, 0.0f, 0.0f);
                }
            }
            schema->parameters.push_back(p);
        }
    }

    if (json.contains("graph")) {
        load_material_graph(json["graph"], schema->graph);
    }

    if (json.contains("blend_mode")) {
        schema->blend_mode = string_to_blend_mode(json["blend_mode"].get<String>());
    }

    if (json.contains("cull_mode")) {
        schema->cull_mode = string_to_cull_mode(json["cull_mode"].get<String>());
    }

    if (json.contains("depth_write"))
        schema->depth_write = json["depth_write"].get<bool>();

    if (json.contains("depth_test"))
        schema->depth_test = json["depth_test"].get<bool>();

    return schema;
}

static uint get_schema_extensions(CString* extensions)
{
    if (extensions) {
        extensions[0] = ".matschema";
    }
    return 1;
}

AssetLoaderAPI MaterialSchema::loader()
{
    auto api                     = AssetLoaderAPI{};
    api.configure                = nullptr;
    api.load                     = load_material_schema;
    api.unload                   = [](void* asset) { delete reinterpret_cast<MaterialSchema*>(asset); };
    api.get_supported_extensions = get_schema_extensions;
    return api;
}
