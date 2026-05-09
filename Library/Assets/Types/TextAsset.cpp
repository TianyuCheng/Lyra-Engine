#include <Lyra/Assets/Types/TextAsset.h>
#include <Lyra/FileIO/VFSAPI.h>

using namespace lyra;

static void* load_text_asset(FileLoader* loader, FSPath path)
{
    auto content = loader->read<char>(path);
    return new TextAsset{String(content.begin(), content.end())};
}

static uint get_text_extensions(CString* extensions)
{
    if (extensions) {
        extensions[0] = ".txt";
    }
    return 1;
}

static bool text_process(JSON& metadata, OSPath source_path, OSPath)
{
    metadata["path"] = reinterpret_cast<const char*>(source_path);
    return true;
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
