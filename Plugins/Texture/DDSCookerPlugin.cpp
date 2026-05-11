#include <gli/gli.hpp>
#include <gli/load.hpp>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Plugin.h>
#include "TextureUtils.h"

using namespace lyra;
using namespace lyra::texture;

static void configure_cooker(AssetServer* manager, const JSON& options) {}

static bool process_dds(JSON& metadata, OSPath source_path, OSPath target_path)
{
    String source_path_str = Path(source_path).string();

    gli::texture tex = gli::load(source_path_str);
    if (tex.empty()) {
        get_logger()->error("Failed to load DDS file: {}", source_path_str);
        return false;
    }

    VkFormat vk_format = gli_to_vk_format(tex.format());
    if (vk_format == VK_FORMAT_UNDEFINED) {
        get_logger()->error("Unsupported DDS format in file: {}", source_path_str);
        return false;
    }

    ktxTexture2*         ktx_tex;
    ktxTextureCreateInfo create_info;
    create_info.glInternalformat = 0;
    create_info.vkFormat         = vk_format;
    create_info.baseWidth        = tex.extent().x;
    create_info.baseHeight       = tex.extent().y;
    create_info.baseDepth        = tex.extent().z;
    create_info.numDimensions    = tex.target() == gli::TARGET_1D ? 1 : (tex.target() == gli::TARGET_3D ? 3 : 2);
    create_info.numLevels        = static_cast<uint32_t>(tex.levels());
    create_info.numLayers        = static_cast<uint32_t>(tex.layers());
    create_info.numFaces         = static_cast<uint32_t>(tex.faces());
    create_info.isArray          = tex.layers() > 1 ? KTX_TRUE : KTX_FALSE;
    create_info.generateMipmaps  = KTX_FALSE;

    KTX_error_code result = ktxTexture2_Create(&create_info, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &ktx_tex);
    if (result != KTX_SUCCESS) {
        get_logger()->error("Failed to create KTX2 texture from DDS: {}", ktxErrorString(result));
        return false;
    }

    for (size_t layer = 0; layer < tex.layers(); ++layer) {
        for (size_t face = 0; face < tex.faces(); ++face) {
            for (size_t level = 0; level < tex.levels(); ++level) {
                ktxTexture_SetImageFromMemory(ktxTexture(ktx_tex), static_cast<uint32_t>(level), static_cast<uint32_t>(layer), static_cast<uint32_t>(face),
                    static_cast<const ktx_uint8_t*>(tex.data(layer, face, level)), tex.size(level));
            }
        }
    }

    const Path final_target_path = get_texture_cache_path(metadata["guid"].get<AssetID>(), target_path);
    return save_to_ktx2(metadata, ktx_tex, final_target_path, target_path, get_logger());
}

static uint get_dds_extensions(CString* extensions)
{
    if (extensions) extensions[0] = ".dds";
    return 1;
}

namespace lyra::texture::cooker::dds
{
    void prepare() {}
    void cleanup() {}
    auto create() -> AssetCookerAPI
    {
        auto api                     = AssetCookerAPI{};
        api.configure                = configure_cooker;
        api.process                  = process_dds;
        api.get_supported_extensions = get_dds_extensions;
        return api;
    }
} // namespace lyra::texture::cooker::dds
