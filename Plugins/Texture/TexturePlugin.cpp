#include <ktx.h>
#include <vulkan/vulkan.h>

#include <Lyra/Common/Macros.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/Render/RHIAPI.h>
#include <Lyra/FileIO/VFSAPI.h>

#include <Lyra/Format/TextureAsset.h>

using namespace lyra;

namespace fs = std::filesystem;

using TextureCookerPlugin = BuiltinPlugin<AssetCookerAPI>;

static GPUTextureFormat to_gpu_texture_format(VkFormat format)
{
    // clang-format off
    switch (format) {
        case VK_FORMAT_R8G8B8A8_UNORM:       return GPUTextureFormat::RGBA8UNORM;
        case VK_FORMAT_R8G8B8A8_SRGB:        return GPUTextureFormat::RGBA8UNORM_SRGB;
        case VK_FORMAT_B8G8R8A8_UNORM:       return GPUTextureFormat::BGRA8UNORM;
        case VK_FORMAT_B8G8R8A8_SRGB:        return GPUTextureFormat::BGRA8UNORM_SRGB;
        case VK_FORMAT_R32G32B32A32_SFLOAT:  return GPUTextureFormat::RGBA32FLOAT;
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK: return GPUTextureFormat::BC1_RGBA_UNORM;
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:  return GPUTextureFormat::BC1_RGBA_UNORM_SRGB;
        case VK_FORMAT_BC2_UNORM_BLOCK:      return GPUTextureFormat::BC2_RGBA_UNORM;
        case VK_FORMAT_BC2_SRGB_BLOCK:       return GPUTextureFormat::BC2_RGBA_UNORM_SRGB;
        case VK_FORMAT_BC3_UNORM_BLOCK:      return GPUTextureFormat::BC3_RGBA_UNORM;
        case VK_FORMAT_BC3_SRGB_BLOCK:       return GPUTextureFormat::BC3_RGBA_UNORM_SRGB;
        case VK_FORMAT_BC4_UNORM_BLOCK:      return GPUTextureFormat::BC4_R_UNORM;
        case VK_FORMAT_BC4_SNORM_BLOCK:      return GPUTextureFormat::BC4_R_SNORM;
        case VK_FORMAT_BC5_UNORM_BLOCK:      return GPUTextureFormat::BC5_RG_UNORM;
        case VK_FORMAT_BC5_SNORM_BLOCK:      return GPUTextureFormat::BC5_RG_SNORM;
        case VK_FORMAT_BC6H_UFLOAT_BLOCK:    return GPUTextureFormat::BC6H_RGB_UFLOAT;
        case VK_FORMAT_BC6H_SFLOAT_BLOCK:    return GPUTextureFormat::BC6H_RGB_FLOAT;
        case VK_FORMAT_BC7_UNORM_BLOCK:      return GPUTextureFormat::BC7_RGBA_UNORM;
        case VK_FORMAT_BC7_SRGB_BLOCK:       return GPUTextureFormat::BC7_RGBA_UNORM_SRGB;
        default: 			     return GPUTextureFormat::RGBA8UNORM;
    }
    // clang-format on
}

static void* load_texture_asset(FileLoader* loader, FSPath path)
{
    auto content = loader->read<uint8_t>(path);

    if (content.empty()) {
        spdlog::error("Failed to read texture file: {}", path);
        return nullptr;
    }

    ktxTexture2*   ktx_texture;
    KTX_error_code result = ktxTexture2_CreateFromMemory(content.data(), content.size(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktx_texture);
    if (result != KTX_SUCCESS) {
        spdlog::error("Failed to create KTX texture from memory: {}", ktxErrorString(result));
        return nullptr;
    }

    auto texture_asset    = new TextureAsset();
    texture_asset->format = to_gpu_texture_format(static_cast<VkFormat>(ktx_texture->vkFormat));
    texture_asset->binary.resize(ktx_texture->dataSize);
    memcpy(texture_asset->binary.data(), ktx_texture->pData, ktx_texture->dataSize);

    for (uint32_t i = 0; i < ktx_texture->numLevels; ++i) {
        ktx_size_t offset;
        ktxTexture_GetImageOffset(ktxTexture(ktx_texture), i, 0, 0, &offset);
        texture_asset->subresources.push_back({static_cast<uint32_t>(offset), ktxTexture_GetRowPitch(reinterpret_cast<ktxTexture*>(ktx_texture), i), ktx_texture->baseWidth >> i, ktx_texture->baseHeight >> i});
    }

    ktxTexture_Destroy(ktxTexture(ktx_texture));
    return texture_asset;
}

static void unload_texture_asset(void* asset)
{
    delete reinterpret_cast<TextureAsset*>(asset);
}

static uint get_supported_loader_extensions(CString* extensions)
{
    static const char* exts[] = {".ktx", ".ktx2"};
    if (extensions) {
        extensions[0] = exts[0];
        extensions[1] = exts[1];
    }
    return 2;
}

static bool ktx_process(JSON& metadata, OSPath source_path, OSPath caches_root)
{
    Path dst = Path(caches_root) / "Textures" / (std::to_string(metadata["guid"].get<AssetID>()) + ".ktx2");

    fs::create_directories(dst.parent_path());
    fs::copy_file(Path(source_path), dst, fs::copy_options::overwrite_existing);

    metadata["path"] = fs::relative(dst, caches_root).string();
    return true;
}

static uint get_ktx_extensions(CString* extensions)
{
    if (extensions) {
        extensions[0] = ".ktx";
        extensions[1] = ".ktx2";
    }
    return 2;
}

namespace lyra::texture::loader
{
    void prepare() {}

    void cleanup() {}

    auto create() -> AssetLoaderAPI
    {
        auto api                     = AssetLoaderAPI{};
        api.configure                = nullptr;
        api.load                     = load_texture_asset;
        api.unload                   = unload_texture_asset;
        api.get_supported_extensions = get_supported_loader_extensions;
        return api;
    }
} // namespace lyra::texture::loader
