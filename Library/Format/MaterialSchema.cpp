#include <absl/strings/ascii.h>
#include <Lyra/Common/Enums.h>
#include <Lyra/FileIO/VFSAPI.h>
#include <Lyra/Assets/AMSServer.h>
#include <Lyra/Format/MaterialSchema.h>

using namespace lyra;

// Helper to map string to MaterialSemantic
static MaterialSemantic string_to_semantic(const String& str)
{
    return magic_enum::enum_cast<MaterialSemantic>(str, magic_enum::case_insensitive).value_or(MaterialSemantic::NONE);
}

// Helper to map string to MaterialParameterType
static MaterialParameterType string_to_param_type(const String& str)
{
    return magic_enum::enum_cast<MaterialParameterType>(str, magic_enum::case_insensitive).value_or(MaterialParameterType::FLOAT);
}

// Helper to map string to MaterialBlendMode
static MaterialBlendMode string_to_blend_mode(const String& str)
{
    return magic_enum::enum_cast<MaterialBlendMode>(str, magic_enum::case_insensitive).value_or(MaterialBlendMode::OPAQUE);
}

// Helper to map string to GPUCullMode
static GPUCullMode string_to_cull_mode(const String& str)
{
    return magic_enum::enum_cast<GPUCullMode>(str, magic_enum::case_insensitive).value_or(GPUCullMode::BACK);
}

static MaterialGraphValueType to_graph_value_type(const String& str)
{
    return magic_enum::enum_cast<MaterialGraphValueType>(str, magic_enum::case_insensitive).value_or(MaterialGraphValueType::NONE);
}

static String to_string(MaterialGraphValueType type)
{
    return absl::AsciiStrToLower(magic_enum::enum_name(type));
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

static JSON to_json(const MaterialGraphProperty& prop)
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

static JSON to_json(const MaterialGraphPin& pin)
{
    JSON json;
    json["id"]            = pin.id;
    json["name"]          = pin.name;
    json["type"]          = to_string(pin.type);
    json["default_value"] = to_json(pin.default_value);
    return json;
}

static MaterialGraphPin parse_graph_pin(const JSON& json)
{
    MaterialGraphPin pin;
    if (!json.contains("id")) {
        spdlog::error("MaterialGraph: Pin is missing 'id'");
        return pin;
    }
    if (!json.contains("name")) {
        spdlog::error("MaterialGraph: Pin {} is missing 'name'", json["id"].dump());
        return pin;
    }
    if (!json.contains("type")) {
        spdlog::error("MaterialGraph: Pin {} is missing 'type'", json["id"].dump());
        return pin;
    }

    pin.id   = json["id"].get<uint>();
    pin.name = json["name"].get<String>();
    pin.type = to_graph_value_type(json["type"].get<String>());
    if (json.contains("default_value")) {
        pin.default_value = parse_graph_property(json["default_value"]);
    }
    return pin;
}

void MaterialGraph::load(const JSON& json)
{
    nodes.clear();
    links.clear();

    if (json.contains("nodes") && json["nodes"].is_array()) {
        for (auto& n_json : json["nodes"]) {
            if (!n_json.contains("id")) {
                spdlog::error("MaterialGraph: Node is missing 'id'");
                continue;
            }
            if (!n_json.contains("type")) {
                spdlog::error("MaterialGraph: Node {} is missing 'type'", n_json["id"].dump());
                continue;
            }

            MaterialGraphNode node;
            node.id   = n_json["id"].get<uint>();
            node.type = n_json["type"].get<String>();

            // Unique ID validation
            bool duplicate = false;
            for (const auto& n : nodes) {
                if (n.id == node.id) {
                    spdlog::error("MaterialGraph: Duplicate node ID detected: {}", node.id);
                    duplicate = true;
                    break;
                }
            }
            if (duplicate) continue;

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

            nodes.push_back(node);
        }
    }

    if (json.contains("links") && json["links"].is_array()) {
        for (auto& l_json : json["links"]) {
            if (!l_json.contains("id")) {
                spdlog::error("MaterialGraph: A link is missing 'id'");
                continue;
            }
            if (!l_json.contains("from_node")) {
                spdlog::error("MaterialGraph: Link {} is missing 'from_node'", l_json["id"].dump());
                continue;
            }
            if (!l_json.contains("from_pin")) {
                spdlog::error("MaterialGraph: Link {} is missing 'from_pin'", l_json["id"].dump());
                continue;
            }
            if (!l_json.contains("to_node")) {
                spdlog::error("MaterialGraph: Link {} is missing 'to_node'", l_json["id"].dump());
                continue;
            }
            if (!l_json.contains("to_pin")) {
                spdlog::error("MaterialGraph: Link {} is missing 'to_pin'", l_json["id"].dump());
                continue;
            }

            MaterialGraphLink link;
            link.id        = l_json["id"].get<uint>();
            link.from_node = l_json["from_node"].get<uint>();
            link.from_pin  = l_json["from_pin"].get<uint>();
            link.to_node   = l_json["to_node"].get<uint>();
            link.to_pin    = l_json["to_pin"].get<uint>();

            // Unique ID validation
            bool duplicate = false;
            for (const auto& l : links) {
                if (l.id == link.id) {
                    spdlog::error("MaterialGraph: Duplicate link ID detected: {}", link.id);
                    duplicate = true;
                    break;
                }
            }
            if (duplicate) continue;

            // Basic link validation
            auto node_exists = [this](uint id) {
                for (const auto& n : nodes)
                    if (n.id == id) return true;
                return false;
            };

            if (!node_exists(link.from_node) || !node_exists(link.to_node)) {
                spdlog::error("MaterialGraph: Link {} references non-existent node ({} -> {})", link.id, link.from_node, link.to_node);
                continue;
            }

            links.push_back(link);
        }
    }
}

JSON MaterialGraph::save() const
{
    JSON json;

    json["nodes"] = JSON::array();
    for (const auto& node : nodes) {
        JSON n_json;
        n_json["id"]   = node.id;
        n_json["type"] = node.type;

        n_json["inputs"] = JSON::array();
        for (const auto& input : node.inputs) {
            n_json["inputs"].push_back(to_json(input));
        }

        n_json["outputs"] = JSON::array();
        for (const auto& output : node.outputs) {
            n_json["outputs"].push_back(to_json(output));
        }

        n_json["properties"] = JSON::object();
        for (const auto& [key, val] : node.properties) {
            n_json["properties"][key] = to_json(val);
        }

        json["nodes"].push_back(n_json);
    }

    json["links"] = JSON::array();
    for (const auto& link : links) {
        JSON l_json;
        l_json["id"]        = link.id;
        l_json["from_node"] = link.from_node;
        l_json["from_pin"]  = link.from_pin;
        l_json["to_node"]   = link.to_node;
        l_json["to_pin"]    = link.to_pin;
        json["links"].push_back(l_json);
    }

    return json;
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
        schema->graph.load(json["graph"]);
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
