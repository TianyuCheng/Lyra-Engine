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

using MaterialLoaderPlugin = BuiltinPlugin<AssetLoaderAPI>;

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
