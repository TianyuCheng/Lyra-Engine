#include <Lyra/Common/Macros.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/FileIO/VFSAPI.h>
#include <Lyra/Format/TextureAsset.h>

using namespace lyra;

// forward declarations for inlined plugins
namespace lyra::texture::cooker::stb
{
    extern AssetCookerAPI create();
    extern void           prepare();
    extern void           cleanup();
} // namespace lyra::texture::cooker::stb

namespace lyra::texture::cooker::exr
{
    extern AssetCookerAPI create();
    extern void           prepare();
    extern void           cleanup();
} // namespace lyra::texture::cooker::exr

namespace lyra::texture::cooker::dds
{
    extern AssetCookerAPI create();
    extern void           prepare();
    extern void           cleanup();
} // namespace lyra::texture::cooker::dds

namespace lyra::texture::cooker::ktx
{
    extern AssetCookerAPI create();
    extern void           prepare();
    extern void           cleanup();
} // namespace lyra::texture::cooker::ktx

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

AssetCookerAPI TextureAsset::stb::cooker()
{
    static Own<TextureCookerPlugin> PLUGIN;
    if (!PLUGIN)
        PLUGIN = std::make_unique<TextureCookerPlugin>(
            lyra::texture::cooker::stb::create,
            lyra::texture::cooker::stb::prepare,
            lyra::texture::cooker::stb::cleanup);
    return *PLUGIN->get_api();
}

AssetCookerAPI TextureAsset::exr::cooker()
{
    static Own<TextureCookerPlugin> PLUGIN;
    if (!PLUGIN)
        PLUGIN = std::make_unique<TextureCookerPlugin>(
            lyra::texture::cooker::exr::create,
            lyra::texture::cooker::exr::prepare,
            lyra::texture::cooker::exr::cleanup);
    return *PLUGIN->get_api();
}

AssetCookerAPI TextureAsset::dds::cooker()
{
    static Own<TextureCookerPlugin> PLUGIN;
    if (!PLUGIN)
        PLUGIN = std::make_unique<TextureCookerPlugin>(
            lyra::texture::cooker::dds::create,
            lyra::texture::cooker::dds::prepare,
            lyra::texture::cooker::dds::cleanup);
    return *PLUGIN->get_api();
}

AssetCookerAPI TextureAsset::ktx::cooker()
{
    static Own<TextureCookerPlugin> PLUGIN;
    if (!PLUGIN)
        PLUGIN = std::make_unique<TextureCookerPlugin>(
            lyra::texture::cooker::ktx::create,
            lyra::texture::cooker::ktx::prepare,
            lyra::texture::cooker::ktx::cleanup);
    return *PLUGIN->get_api();
}
