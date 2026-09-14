#include <fstream>

#include <Lyra/Common/Macros.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/FileIO/VFSAPI.h>
#include <Lyra/Format/MaterialAsset.h>

using namespace lyra;

// forward declarations for inlined plugins
namespace lyra::material::loader
{
    extern AssetLoaderAPI create();
    extern void           prepare();
    extern void           cleanup();
} // namespace lyra::material::loader

namespace lyra::material::saver
{
    extern AssetSaverAPI create();
    extern void          prepare();
    extern void          cleanup();
} // namespace lyra::material::saver

using MaterialLoaderPlugin = BuiltinPlugin<AssetLoaderAPI>;
using MaterialSaverPlugin  = BuiltinPlugin<AssetSaverAPI>;

AssetLoaderAPI MaterialAsset::loader()
{
    static Own<MaterialLoaderPlugin> PLUGIN;
    if (!PLUGIN)
        PLUGIN = std::make_unique<MaterialLoaderPlugin>(
            lyra::material::loader::create,
            lyra::material::loader::prepare,
            lyra::material::loader::cleanup);
    return *PLUGIN->get_api();
}

AssetSaverAPI MaterialAsset::saver()
{
    static Own<MaterialSaverPlugin> PLUGIN;
    if (!PLUGIN)
        PLUGIN = std::make_unique<MaterialSaverPlugin>(
            lyra::material::saver::create,
            lyra::material::saver::prepare,
            lyra::material::saver::cleanup);
    return *PLUGIN->get_api();
}

