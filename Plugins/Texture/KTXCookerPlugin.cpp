#include <gli/gli.hpp>
#include <gli/load.hpp>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Plugin.h>
#include "TextureUtils.h"
#include "ThumbnailUtils.h"

using namespace lyra;
using namespace lyra::texture;

static void configure_cooker(AssetServer* manager, const JSON& options) {}

static bool process_ktx(JSON& metadata, OSPath source_path, OSPath target_path)
{
    String source_path_str = Path(source_path).string();
    Path   dst             = get_texture_cache_path(metadata["guid"].get<AssetID>(), target_path);

    if (!fs::exists(Path(source_path_str))) {
        get_logger()->error("Source KTX file does not exist: {}", source_path_str);
        return false;
    }

    {
        gli::texture tex = gli::load(source_path_str);
        if (!tex.empty()) {
            if (!gli::is_compressed(tex.format())) {
                if (tex.target() == gli::TARGET_2D) {
                    gli::texture2d tex2d(tex);
                    // For uncompressed, we only handle RGBA8 for thumbnail for now to avoid gli::convert ambiguity
                    if (tex2d.format() == gli::FORMAT_RGBA8_UNORM_PACK8) {
                        generate_thumbnail_from_pixels(metadata, tex2d.data(0, 0, 0), tex2d.extent(0).x, tex2d.extent(0).y, sizeof(uint8_t), VK_FORMAT_R8G8B8A8_UNORM, target_path);
                    }
                }
            }
        }
    }

    try {
        fs::create_directories(dst.parent_path());
        fs::copy_file(Path(source_path_str), dst, fs::copy_options::overwrite_existing);
    } catch (const std::exception& e) {
        get_logger()->error("Failed to copy KTX file to cache: {} (error: {})", source_path_str, e.what());
        return false;
    }

    metadata["path"] = fs::relative(dst, target_path).string();
    return true;
}

static uint get_ktx_extensions(CString* extensions)
{
    static const char* exts[] = {".ktx", ".ktx2"};
    if (extensions) {
        extensions[0] = exts[0];
        extensions[1] = exts[1];
    }
    return 2;
}

namespace lyra::texture::cooker::ktx
{
    void prepare() {}
    void cleanup() {}
    auto create() -> AssetCookerAPI
    {
        auto api                     = AssetCookerAPI{};
        api.configure                = configure_cooker;
        api.process                  = process_ktx;
        api.get_supported_extensions = get_ktx_extensions;
        return api;
    }
} // namespace lyra::texture::cooker::ktx
