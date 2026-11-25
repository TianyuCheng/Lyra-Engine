#include <Lyra/Utilities/Plugin.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/Assets/AMSPreview.h>
#include "BatchPreviewDispatcher.h"

using namespace lyra;

static void configure_preview(const AssetConfigContext& context)
{
    preview::BatchPreviewDispatcher::instance().configure(context);
}

static PreviewTexture render_scene_api(const PreviewScene& scene)
{
    return preview::BatchPreviewDispatcher::instance().render_scene(scene);
}

static Future<Path> generate_thumbnail_api(const PreviewScene& scene, JSON& metadata)
{
    return preview::BatchPreviewDispatcher::instance().generate_thumbnail(scene, metadata);
}

static void flush_preview_api()
{
    preview::BatchPreviewDispatcher::instance().flush();
}

namespace lyra::preview
{
    void prepare() {}
    void cleanup()
    {
        BatchPreviewDispatcher::shutdown();
    }
    auto create() -> AssetPreviewAPI
    {
        AssetPreviewAPI api{};
        api.configure          = configure_preview;
        api.render_scene       = render_scene_api;
        api.generate_thumbnail = generate_thumbnail_api;
        api.flush              = flush_preview_api;
        return api;
    }
} // namespace lyra::preview
