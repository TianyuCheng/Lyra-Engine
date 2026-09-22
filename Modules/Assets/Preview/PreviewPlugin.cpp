#include <Lyra/Utilities/Plugin.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/Assets/AMSPreview.h>
#include "SoftwareRasterizer.h"

using namespace lyra;

static void configure_preview(AssetServer* manager, const JSON& options)
{
    preview::configure(manager, options);
}

static auto render_scene_api(const PreviewScene& scene) -> PreviewTexture
{
    return preview::rasterize_scene(scene);
}

static auto generate_thumbnail_api(
    const PreviewScene& scene,
    JSON&               metadata) -> Future<Path>
{
    Promise<Path> p;
    Path path = preview::generate_scene_thumbnail(scene, metadata);
    p.set_value(path);
    return p.get_future();
}

namespace lyra::preview
{
    void prepare() {}
    void cleanup() {}
    auto create() -> AssetPreviewAPI
    {
        AssetPreviewAPI api{};
        api.configure          = configure_preview;
        api.render_scene       = render_scene_api;
        api.generate_thumbnail = generate_thumbnail_api;
        return api;
    }
}
