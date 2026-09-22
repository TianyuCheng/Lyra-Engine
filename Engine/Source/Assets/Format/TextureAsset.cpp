#include <Lyra/Utilities/Macros.h>
#include <Lyra/Utilities/Plugin.h>
#include <Lyra/FileSystem/VFSAPI.h>
#include <Lyra/Assets/Format/TextureAsset.h>

using namespace lyra;

// forward declarations for inlined plugins
FORWARD_DECLARE_API(lyra::texture::cooker::stb, AssetCookerAPI)
FORWARD_DECLARE_API(lyra::texture::cooker::exr, AssetCookerAPI)
FORWARD_DECLARE_API(lyra::texture::cooker::dds, AssetCookerAPI)
FORWARD_DECLARE_API(lyra::texture::cooker::ktx, AssetCookerAPI)

// forward declarations for inlined plugins
FORWARD_DECLARE_API(lyra::texture::loader, AssetLoaderAPI)

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
