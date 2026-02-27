#include <Lyra/Common/Plugin.h>

#include "TextureAsset.h"

using namespace lyra;

using TextureLoaderPlugin = Plugin<AssetLoaderAPI>;
using TextureCookerPlugin = Plugin<AssetCookerAPI>;

static JSON ktx_process(OSPath source_path, OSPath target_path)
{
    MAYBE_UNUSED(target_path);

    // for ktx, ktx2, we don't need to do anything except for set metadata
    JSON metadata;
    metadata["path"] = reinterpret_cast<const char*>(source_path);
    return metadata;
}

static uint get_ktx_extensions(CString* extensions)
{
    if (extensions) {
        extensions[0] = ".ktx";
        extensions[1] = ".ktx2";
    }
    return 2;
}

AssetLoaderAPI TextureAsset::loader()
{
    static Own<TextureLoaderPlugin> TEXTURE_LOADER_PLUGIN;

    // NOTE: we will only need to load ktx/ktx2 formats
    if (!TEXTURE_LOADER_PLUGIN)
        TEXTURE_LOADER_PLUGIN = std::make_unique<TextureLoaderPlugin>("lyra-ktx");

    return *TEXTURE_LOADER_PLUGIN->get_api();
}

AssetCookerAPI TextureAsset::stb::cooker()
{
    // NOTE: we will need to cook jpg/png/hdr
    static Own<TextureCookerPlugin> PLUGIN;
    if (!PLUGIN) PLUGIN = std::make_unique<TextureCookerPlugin>("lyra-stb");
    return *PLUGIN->get_api();
}

AssetCookerAPI TextureAsset::exr::cooker()
{
    // NOTE: we will need to cook exr
    static Own<TextureCookerPlugin> PLUGIN;
    if (!PLUGIN) PLUGIN = std::make_unique<TextureCookerPlugin>("lyra-exr");
    return *PLUGIN->get_api();
}

AssetCookerAPI TextureAsset::dds::cooker()
{
    // NOTE: we will need to cook dds
    static Own<TextureCookerPlugin> PLUGIN;
    if (!PLUGIN) PLUGIN = std::make_unique<TextureCookerPlugin>("lyra-dds");
    return *PLUGIN->get_api();
}

AssetCookerAPI TextureAsset::ktx::cooker()
{
    auto api                     = AssetCookerAPI{};
    api.configure                = nullptr;
    api.process                  = ktx_process;
    api.get_supported_extensions = get_ktx_extensions;
    return api;
}
