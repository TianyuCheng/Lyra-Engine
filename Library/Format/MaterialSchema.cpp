#include <Lyra/Common/Macros.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/FileIO/VFSAPI.h>
#include <Lyra/Format/MaterialSchema.h>

using namespace lyra;

// forward declarations for inlined plugins
namespace lyra::matschema::loader
{
    extern AssetLoaderAPI create();
    extern void           prepare();
    extern void           cleanup();
} // namespace lyra::matschema::loader

using MaterialSchemaLoaderPlugin = BuiltinPlugin<AssetLoaderAPI>;

AssetLoaderAPI MaterialSchema::loader()
{
    static Own<MaterialSchemaLoaderPlugin> PLUGIN;
    if (!PLUGIN)
        PLUGIN = std::make_unique<MaterialSchemaLoaderPlugin>(
            lyra::matschema::loader::create,
            lyra::matschema::loader::prepare,
            lyra::matschema::loader::cleanup);
    return *PLUGIN->get_api();
}
