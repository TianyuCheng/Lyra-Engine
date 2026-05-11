#include <Lyra/Common/Plugin.h>
#include <Lyra/Assets/AMSAPI.h>

using namespace lyra;

extern void* load_material_schema(FileLoader* loader, FSPath path);
extern void  unload_material_schema(void* asset);
extern uint  get_schema_extensions(CString* extensions);

namespace lyra::matschema::loader
{
    void prepare() {}

    void cleanup() {}

    auto create() -> AssetLoaderAPI
    {
        auto api                     = AssetLoaderAPI{};
        api.configure                = nullptr;
        api.load                     = load_material_schema;
        api.unload                   = unload_material_schema;
        api.get_supported_extensions = get_schema_extensions;
        return api;
    }
} // namespace lyra::matschema::loader
