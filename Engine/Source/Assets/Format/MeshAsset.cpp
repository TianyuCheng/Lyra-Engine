#include <fstream>

#include <Lyra/Utilities/Macros.h>
#include <Lyra/Utilities/Plugin.h>
#include <Lyra/FileSystem/VFSAPI.h>
#include <Lyra/Assets/Format/MeshAsset.h>

using namespace lyra;

// forward declarations for inlined plugins
FORWARD_DECLARE_API(lyra::mesh::loader, AssetLoaderAPI)
FORWARD_DECLARE_API(lyra::mesh::saver, AssetSaverAPI)

using MeshLoaderPlugin = BuiltinPlugin<AssetLoaderAPI>;
using MeshSaverPlugin  = BuiltinPlugin<AssetSaverAPI>;

AssetLoaderAPI MeshAsset::loader()
{
    static Own<MeshLoaderPlugin> PLUGIN;
    if (!PLUGIN)
        PLUGIN = std::make_unique<MeshLoaderPlugin>(
            lyra::mesh::loader::create,
            lyra::mesh::loader::prepare,
            lyra::mesh::loader::cleanup);
    return *PLUGIN->get_api();
}

AssetSaverAPI MeshAsset::saver()
{
    static Own<MeshSaverPlugin> PLUGIN;
    if (!PLUGIN)
        PLUGIN = std::make_unique<MeshSaverPlugin>(
            lyra::mesh::saver::create,
            lyra::mesh::saver::prepare,
            lyra::mesh::saver::cleanup);
    return *PLUGIN->get_api();
}

