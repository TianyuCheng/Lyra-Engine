#include <Lyra/Common/Plugin.h>

#include <Lyra/Format/ModelAsset.h>

using namespace lyra;

using ModelLoaderPlugin = Plugin<AssetLoaderAPI>;
using ModelCookerPlugin = Plugin<AssetCookerAPI>;

AssetLoaderAPI ModelAsset::loader()
{
    static Own<ModelLoaderPlugin> PLUGIN;
    if (!PLUGIN) PLUGIN = std::make_unique<ModelLoaderPlugin>("lyra-model");
    return *PLUGIN->get_api();
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
