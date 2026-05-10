#include "helper.h"

namespace fs = std::filesystem;

using namespace lyra;

TEST_CASE("mat::material_graph_load")
{
    // setup temporary directory for tests
    auto temp_dir = fs::temp_directory_path() / "lyra_mat_test";
    fs::create_directories(temp_dir);

    auto schema_path = temp_dir / "test.matschema";

    // Create a dummy material schema with a graph
    {
        JSON graph;

        // Add a node
        JSON node;
        node["id"]   = 1;
        node["type"] = "Add";

        JSON input;
        input["id"]            = 10;
        input["name"]          = "A";
        input["type"]          = "float";
        input["default_value"] = 1.0f;
        node["inputs"]         = JSON::array({input});

        JSON output;
        output["id"]    = 11;
        output["name"]  = "Out";
        output["type"]  = "float";
        node["outputs"] = JSON::array({output});

        node["properties"]["op"] = "sum";

        graph["nodes"] = JSON::array({node});

        // Add a link
        JSON link;
        link["id"]        = 100;
        link["from_node"] = 1;
        link["from_pin"]  = 11;
        link["to_node"]   = 2;
        link["to_pin"]    = 20;
        graph["links"]    = JSON::array({link});

        JSON schema;
        schema["schema_id"]    = 42;
        schema["display_name"] = "Test Material";
        schema["graph"]        = graph;

        std::ofstream f(schema_path);
        f << schema.dump();
        f.close();
    }

    // initialize fileloader
    FileLoader loader(FSLoader::NATIVE);
    loader.mount("/", temp_dir.string().c_str(), 0);

    SUBCASE("verify loaded graph")
    {
        auto loader_api = MaterialSchema::loader();
        auto asset      = static_cast<MaterialSchema*>(loader_api.load(&loader, "/test.matschema"));

        REQUIRE_NE(asset, nullptr);
        CHECK_EQ(asset->schema_id, 42);
        CHECK_EQ(asset->display_name, "Test Material");

        // Verify nodes
        REQUIRE_EQ(asset->graph.nodes.size(), 1);
        auto& node = asset->graph.nodes[0];
        CHECK_EQ(node.id, 1);
        CHECK_EQ(node.type, "Add");

        // Verify pins
        REQUIRE_EQ(node.inputs.size(), 1);
        CHECK_EQ(node.inputs[0].id, 10);
        CHECK_EQ(node.inputs[0].name, "A");
        CHECK_EQ(node.inputs[0].type, MaterialGraphValueType::FLOAT);
        CHECK_EQ(std::get<float>(node.inputs[0].default_value.value), 1.0f);

        // Verify properties
        REQUIRE(node.properties.contains("op"));
        CHECK_EQ(std::get<String>(node.properties.at("op").value), "sum");

        // Verify links
        REQUIRE_EQ(asset->graph.links.size(), 1);
        auto& link = asset->graph.links[0];
        CHECK_EQ(link.id, 100);
        CHECK_EQ(link.from_node, 1);
        CHECK_EQ(link.from_pin, 11);
        CHECK_EQ(link.to_node, 2);
        CHECK_EQ(link.to_pin, 20);

        loader_api.unload(asset);
    }

    // cleanup
    fs::remove_all(temp_dir);
}
