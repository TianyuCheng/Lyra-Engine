#include "TextAsset.h"
#include <Lyra/FileIO/VFSAPI.h>

using namespace lyra;

static void* load_text_asset(FileLoader* loader, const JSON& metadata)
{
    auto path    = metadata["path"].template get<String>();
    auto content = loader->read<char>(path.c_str());
    return new TextAsset{String(content.begin(), content.end())};
}

static uint get_text_extensions(CString* extensions)
{
    if (extensions) {
        extensions[0] = ".txt";
    }
    return 1;
}

static JSON text_process(OSPath source_path, OSPath)
{
    JSON metadata;
    metadata["path"] = Path(source_path).string();
    return metadata;
}

AssetLoaderAPI TextAsset::loader()
{
    auto api                     = AssetLoaderAPI{};
    api.configure                = nullptr;
    api.load                     = load_text_asset;
    api.unload                   = [](void* asset) { delete reinterpret_cast<TextAsset*>(asset); };
    api.get_supported_extensions = get_text_extensions;
    return api;
}

AssetCookerAPI TextAsset::cooker()
{
    auto api                     = AssetCookerAPI{};
    api.configure                = nullptr;
    api.process                  = text_process;
    api.get_supported_extensions = get_text_extensions;
    return api;
}
