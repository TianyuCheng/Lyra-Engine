#include <Lyra/Common/Macros.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/FileIO/VFSAPI.h>
#include <Lyra/Format/ModelAsset.h>

using namespace lyra;

static void* load_model_asset(FileLoader* loader, FSPath path)
{
    auto content = loader->read<char>(path);
    if (content.empty()) return nullptr;

    JSON json;
    try {
        json = JSON::parse(content.begin(), content.end());
    } catch (...) {
        return nullptr;
    }

    if (!json.contains("nodes") || !json["nodes"].is_array()) return nullptr;

    auto asset  = new ModelAsset();
    asset->root = json.value("root", 0u);
    asset->nodes.reserve(json["nodes"].size());

    for (const auto& n : json["nodes"]) {
        auto& node = asset->nodes.emplace_back();
        node.name  = n.value("name", "");

        if (n.contains("transform")) {
            auto& t = n["transform"];
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    node.transform[i][j] = t[i][j];
        }

        if (n.contains("mesh")) node.mesh = AssetHandle<MeshAsset>(std::stoull(n["mesh"].get<String>()));
        if (n.contains("material")) node.material = AssetHandle<MaterialAsset>(std::stoull(n["material"].get<String>()));
        if (n.contains("children") && n["children"].is_array()) {
            for (const auto& c : n["children"])
                node.children.push_back(c.get<uint>());
        }
    }

    return asset;
}

static void unload_model_asset(void* asset)
{
    delete reinterpret_cast<ModelAsset*>(asset);
}

static uint get_model_extensions(CString* extensions)
{
    if (extensions) {
        extensions[0] = ".model";
    }
    return 1;
}

namespace lyra::model::loader
{
    void prepare() {}
    void cleanup() {}
    auto create() -> AssetLoaderAPI
    {
        auto api                     = AssetLoaderAPI{};
        api.load                     = load_model_asset;
        api.unload                   = unload_model_asset;
        api.get_supported_extensions = get_model_extensions;
        return api;
    }
} // namespace lyra::model::loader
