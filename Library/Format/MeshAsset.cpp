#include <Lyra/Common/Macros.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/FileIO/VFSAPI.h>
#include <Lyra/Format/MeshAsset.h>

using namespace lyra;

// forward declarations for inlined plugins
namespace lyra::mesh::loader
{
    extern AssetLoaderAPI create();
    extern void           prepare();
    extern void           cleanup();
} // namespace lyra::mesh::loader

using MeshLoaderPlugin = BuiltinPlugin<AssetLoaderAPI>;

AssetLoaderAPI MeshAsset::loader()
{
    static Own<MeshLoaderPlugin> PLUGIN;
    if (!PLUGIN)
        PLUGIN = std::make_unique<MeshLoaderPlugin>(
            lyra::mesh::loader::create,
            lyra::mesh::loader::prepare,
            lyra::mesh::loader::cleanup);
    return *PLUGIN->get_api();
}
