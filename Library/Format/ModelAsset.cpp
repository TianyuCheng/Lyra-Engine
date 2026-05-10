#include <Lyra/Common/Plugin.h>

#include <Lyra/Format/ModelAsset.h>

using namespace lyra;

using ModelLoaderPlugin = Plugin<AssetLoaderAPI>;
using ModelCookerPlugin = Plugin<AssetCookerAPI>;

AssetLoaderAPI ModelAsset::loader()
{
    static Own<ModelLoaderPlugin> MODEL_LOADER_PLUGIN;

    if (!MODEL_LOADER_PLUGIN)
        MODEL_LOADER_PLUGIN = std::make_unique<ModelLoaderPlugin>("lyra-model");

    return *MODEL_LOADER_PLUGIN->get_api();
}

AssetCookerAPI ModelAsset::stl::cooker()
{
    static Own<ModelCookerPlugin> PLUGIN;
    if (!PLUGIN) PLUGIN = std::make_unique<ModelCookerPlugin>("lyra-stl");
    return *PLUGIN->get_api();
}

AssetCookerAPI ModelAsset::obj::cooker()
{
    static Own<ModelCookerPlugin> PLUGIN;
    if (!PLUGIN) PLUGIN = std::make_unique<ModelCookerPlugin>("lyra-obj");
    return *PLUGIN->get_api();
}

AssetCookerAPI ModelAsset::gltf::cooker()
{
    static Own<ModelCookerPlugin> PLUGIN;
    if (!PLUGIN) PLUGIN = std::make_unique<ModelCookerPlugin>("lyra-gltf");
    return *PLUGIN->get_api();
}
