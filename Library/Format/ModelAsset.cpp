#include <Lyra/Common/Plugin.h>

#include <Lyra/Format/ModelAsset.h>

using namespace lyra;

// forward declarations
namespace lyra::model::loader {
    extern AssetLoaderAPI create();
    extern void prepare();
    extern void cleanup();
}
namespace lyra::stl::cooker {
    extern AssetCookerAPI create();
    extern void prepare();
    extern void cleanup();
}
namespace lyra::obj::cooker {
    extern AssetCookerAPI create();
    extern void prepare();
    extern void cleanup();
}
namespace lyra::gltf::cooker {
    extern AssetCookerAPI create();
    extern void prepare();
    extern void cleanup();
}

using ModelLoaderPlugin = BuiltinPlugin<AssetLoaderAPI>;
using ModelCookerPlugin = BuiltinPlugin<AssetCookerAPI>;

AssetLoaderAPI ModelAsset::loader()
{
    static Own<ModelLoaderPlugin> PLUGIN;
    if (!PLUGIN) PLUGIN = std::make_unique<ModelLoaderPlugin>(
        lyra::model::loader::create,
        lyra::model::loader::prepare,
        lyra::model::loader::cleanup
    );
    return *PLUGIN->get_api();
}

AssetCookerAPI ModelAsset::stl::cooker()
{
    static Own<ModelCookerPlugin> PLUGIN;
    if (!PLUGIN) PLUGIN = std::make_unique<ModelCookerPlugin>(
        lyra::stl::cooker::create,
        lyra::stl::cooker::prepare,
        lyra::stl::cooker::cleanup
    );
    return *PLUGIN->get_api();
}

AssetCookerAPI ModelAsset::obj::cooker()
{
    static Own<ModelCookerPlugin> PLUGIN;
    if (!PLUGIN) PLUGIN = std::make_unique<ModelCookerPlugin>(
        lyra::obj::cooker::create,
        lyra::obj::cooker::prepare,
        lyra::obj::cooker::cleanup
    );
    return *PLUGIN->get_api();
}

AssetCookerAPI ModelAsset::gltf::cooker()
{
    static Own<ModelCookerPlugin> PLUGIN;
    if (!PLUGIN) PLUGIN = std::make_unique<ModelCookerPlugin>(
        lyra::gltf::cooker::create,
        lyra::gltf::cooker::prepare,
        lyra::gltf::cooker::cleanup
    );
    return *PLUGIN->get_api();
}
