#include <Lyra/Common/Logger.h>
#include <Lyra/Format/MaterialSchema.h>
#include "MatSchemaUtils.h"

using namespace lyra;
using namespace lyra::matschema;

static JSON to_json(const MaterialGraphPin& pin)
{
    JSON json;
    json["id"]            = pin.id;
    json["name"]          = pin.name;
    json["type"]          = matschema::to_string(pin.type);
    json["default_value"] = matschema::to_json(pin.default_value);
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
    pin.type = matschema::to_graph_value_type(json["type"].get<String>());
    if (json.contains("default_value")) {
        pin.default_value = matschema::parse_graph_property(json["default_value"]);
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

            // unique id validation
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
                    node.properties[key] = matschema::parse_graph_property(val);
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

            // unique id validation
            bool duplicate = false;
            for (const auto& l : links) {
                if (l.id == link.id) {
                    spdlog::error("MaterialGraph: Duplicate link ID detected: {}", link.id);
                    duplicate = true;
                    break;
                }
            }
            if (duplicate) continue;

            // basic link validation
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
            n_json["properties"][key] = matschema::to_json(val);
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
