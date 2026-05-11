#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Plugin.h>
#include "TextureUtils.h"

using namespace lyra;
using namespace lyra::texture;

static void configure_cooker(AssetServer* manager, const JSON& options) {}

static bool process_ktx(JSON& metadata, OSPath source_path, OSPath target_path)
{
    String source_path_str = String(reinterpret_cast<const char*>(source_path));
    Path   dst             = get_texture_cache_path(metadata["guid"].get<AssetID>(), target_path);

    fs::create_directories(dst.parent_path());
    fs::copy_file(Path(source_path_str), dst, fs::copy_options::overwrite_existing);

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
