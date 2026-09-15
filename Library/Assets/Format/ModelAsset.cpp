#include <fstream>
#include <sstream>
#include <functional>

#include <Lyra/Common/Macros.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/Assets/Format/ModelAsset.h>

using namespace lyra;

// forward declarations
FORWARD_DECLARE_API(lyra::model::loader, AssetLoaderAPI)
FORWARD_DECLARE_API(lyra::stl::cooker, AssetCookerAPI)
FORWARD_DECLARE_API(lyra::obj::cooker, AssetCookerAPI)
FORWARD_DECLARE_API(lyra::gltf::cooker, AssetCookerAPI)
FORWARD_DECLARE_API(lyra::model::saver, AssetSaverAPI)

using ModelLoaderPlugin = BuiltinPlugin<AssetLoaderAPI>;
using ModelSaverPlugin  = BuiltinPlugin<AssetSaverAPI>;
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

AssetSaverAPI ModelAsset::saver()
{
    static Own<ModelSaverPlugin> PLUGIN;
    if (!PLUGIN) PLUGIN = std::make_unique<ModelSaverPlugin>(
        lyra::model::saver::create,
        lyra::model::saver::prepare,
        lyra::model::saver::cleanup
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


