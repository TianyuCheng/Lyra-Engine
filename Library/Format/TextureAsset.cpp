#include <Lyra/Common/Macros.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/FileIO/VFSAPI.h>
#include <Lyra/Format/TextureAsset.h>

using namespace lyra;

// forward declarations for inlined plugins
namespace lyra::texture::cooker
{
    extern AssetCookerAPI create();
    extern void           prepare();
    extern void           cleanup();
} // namespace lyra::texture::cooker

// forward declarations for inlined plugins
namespace lyra::texture::loader
{
    extern AssetLoaderAPI create();
    extern void           prepare();
    extern void           cleanup();
} // namespace lyra::texture::loader

using TextureCookerPlugin = BuiltinPlugin<AssetCookerAPI>;
using TextureLoaderPlugin = BuiltinPlugin<AssetLoaderAPI>;

AssetLoaderAPI TextureAsset::loader()
{
    static Own<TextureLoaderPlugin> PLUGIN;
    if (!PLUGIN)
        PLUGIN = std::make_unique<TextureLoaderPlugin>(
            lyra::texture::loader::create,
            lyra::texture::loader::prepare,
            lyra::texture::loader::cleanup);
    return *PLUGIN->get_api();
}

AssetCookerAPI TextureAsset::cooker()
{
    static Own<TextureCookerPlugin> PLUGIN;
    if (!PLUGIN)
        PLUGIN = std::make_unique<TextureCookerPlugin>(
            lyra::texture::cooker::create,
            lyra::texture::cooker::prepare,
            lyra::texture::cooker::cleanup);
    return *PLUGIN->get_api();
}
