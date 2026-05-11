#include <Lyra/Common/Logger.h>
#include <Lyra/FileIO/VFSAPI.h>
#include <Lyra/Format/MaterialSchema.h>
#include "MatSchemaUtils.h"

using namespace lyra;
using namespace lyra::matschema;

static Logger get_logger()
{
    static Logger logger = create_logger("MatSchemaLoader", LogLevel::trace);
    return logger;
}

static void load_schema_parameters(MaterialSchema* schema, const JSON& json)
{
    if (json.contains("parameters") && json["parameters"].is_array()) {
        for (auto& p_json : json["parameters"]) {
            MaterialParameter p;
            p.name = p_json["name"].get<String>();
            p.type = matschema::string_to_param_type(p_json["type"].get<String>());

            if (p_json.contains("semantic")) {
                p.semantic = matschema::string_to_semantic(p_json["semantic"].get<String>());
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
}

static void load_schema_states(MaterialSchema* schema, const JSON& json)
{
    if (json.contains("blend_mode")) {
        schema->blend_mode = matschema::string_to_blend_mode(json["blend_mode"].get<String>());
    }

    if (json.contains("cull_mode")) {
        schema->cull_mode = matschema::string_to_cull_mode(json["cull_mode"].get<String>());
    }

    if (json.contains("depth_write"))
        schema->depth_write = json["depth_write"].get<bool>();

    if (json.contains("depth_test"))
        schema->depth_test = json["depth_test"].get<bool>();
}

void* load_material_schema(FileLoader* loader, FSPath path)
{
    auto content = loader->read<char>(path);
    if (content.empty()) {
        get_logger()->error("failed to read material schema file: {}", path);
        return nullptr;
    }

    JSON json;
    try {
        json = JSON::parse(content.begin(), content.end());
    } catch (const std::exception& e) {
        get_logger()->error("failed to parse material schema JSON: {} (error: {})", path, e.what());
        return nullptr;
    }

    auto schema = new MaterialSchema();

    if (json.contains("schema_id"))
        schema->schema_id = json["schema_id"].get<uint>();

    if (json.contains("display_name"))
        schema->display_name = json["display_name"].get<String>();

    load_schema_parameters(schema, json);

    if (json.contains("graph")) {
        schema->graph.load(json["graph"]);
    }

    load_schema_states(schema, json);

    return schema;
}

void unload_material_schema(void* asset)
{
    delete reinterpret_cast<MaterialSchema*>(asset);
}

uint get_schema_extensions(CString* extensions)
{
    if (extensions) {
        extensions[0] = ".matschema";
    }
    return 1;
}
