#include <Lyra/Common/Plugin.h>
#include <Lyra/Format/SceneAsset.h>

using namespace lyra;

// forward declarations
namespace lyra::scene::loader
{
    extern AssetLoaderAPI create();
    extern void           prepare();
    extern void           cleanup();
} // namespace lyra::scene::loader

namespace lyra::scene::saver
{
    extern AssetSaverAPI create();
    extern void          prepare();
    extern void          cleanup();
} // namespace lyra::scene::saver

using SceneLoaderPlugin = BuiltinPlugin<AssetLoaderAPI>;
using SceneSaverPlugin  = BuiltinPlugin<AssetSaverAPI>;

AssetLoaderAPI SceneAsset::loader()
{
    static Own<SceneLoaderPlugin> PLUGIN;
    if (!PLUGIN) PLUGIN = std::make_unique<SceneLoaderPlugin>(
                     lyra::scene::loader::create,
                     lyra::scene::loader::prepare,
                     lyra::scene::loader::cleanup);
    return *PLUGIN->get_api();
}

AssetSaverAPI SceneAsset::saver()
{
    static Own<SceneSaverPlugin> PLUGIN;
    if (!PLUGIN) PLUGIN = std::make_unique<SceneSaverPlugin>(
                     lyra::scene::saver::create,
                     lyra::scene::saver::prepare,
                     lyra::scene::saver::cleanup);
    return *PLUGIN->get_api();
}

SceneAsset SceneAsset::from_model(const ModelAsset& model)
{
    SceneAsset scene;
    scene.root = model.root;
    scene.nodes.reserve(model.nodes.size());
    for (const auto& n : model.nodes) {
        SceneAsset::Node sn;
        sn.name      = n.name;
        sn.transform = n.transform;
        sn.mesh      = n.mesh;
        sn.material  = n.material;
        sn.children  = n.children;
        scene.nodes.push_back(sn);
    }
    return scene;
}

