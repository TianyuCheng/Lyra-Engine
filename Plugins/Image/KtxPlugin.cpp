#include <cmath>
#include <ktx.h>
#include <vulkan/vulkan.h>

#include <Lyra/Common/Macros.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/Assets/Assets.h>
#include <Lyra/Render/RHIAPI.h>

using namespace lyra;

static Logger logger = create_logger("KtxLoader", LogLevel::trace);

Logger get_logger() { return logger; }

static GPUTextureFormat to_gpu_texture_format(VkFormat format)
{
    switch (format) {
        case VK_FORMAT_R8G8B8A8_UNORM: return GPUTextureFormat::RGBA8UNORM;
        case VK_FORMAT_R8G8B8A8_SRGB:  return GPUTextureFormat::RGBA8UNORM_SRGB;
        case VK_FORMAT_B8G8R8A8_UNORM: return GPUTextureFormat::BGRA8UNORM;
        case VK_FORMAT_B8G8R8A8_SRGB:  return GPUTextureFormat::BGRA8UNORM_SRGB;
        case VK_FORMAT_R32G32B32A32_SFLOAT: return GPUTextureFormat::RGBA32FLOAT;
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK: return GPUTextureFormat::BC1_RGBA_UNORM;
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK: return GPUTextureFormat::BC1_RGBA_UNORM_SRGB;
        case VK_FORMAT_BC2_UNORM_BLOCK: return GPUTextureFormat::BC2_RGBA_UNORM;
        case VK_FORMAT_BC2_SRGB_BLOCK: return GPUTextureFormat::BC2_RGBA_UNORM_SRGB;
        case VK_FORMAT_BC3_UNORM_BLOCK: return GPUTextureFormat::BC3_RGBA_UNORM;
        case VK_FORMAT_BC3_SRGB_BLOCK: return GPUTextureFormat::BC3_RGBA_UNORM_SRGB;
        case VK_FORMAT_BC4_UNORM_BLOCK: return GPUTextureFormat::BC4_R_UNORM;
        case VK_FORMAT_BC4_SNORM_BLOCK: return GPUTextureFormat::BC4_R_SNORM;
        case VK_FORMAT_BC5_UNORM_BLOCK: return GPUTextureFormat::BC5_RG_UNORM;
        case VK_FORMAT_BC5_SNORM_BLOCK: return GPUTextureFormat::BC5_RG_SNORM;
        case VK_FORMAT_BC6H_UFLOAT_BLOCK: return GPUTextureFormat::BC6H_RGB_UFLOAT;
        case VK_FORMAT_BC6H_SFLOAT_BLOCK: return GPUTextureFormat::BC6H_RGB_FLOAT;
        case VK_FORMAT_BC7_UNORM_BLOCK: return GPUTextureFormat::BC7_RGBA_UNORM;
        case VK_FORMAT_BC7_SRGB_BLOCK: return GPUTextureFormat::BC7_RGBA_UNORM_SRGB;
        default: return GPUTextureFormat::RGBA8UNORM;
    }
}

static void configure_loader(AssetServer* manager, const JSON& options)
{
    get_logger()->set_level(parse_log_level_from_env("LYRA_TEXTURE_VERBOSITY"));
}

static void* load_texture_asset(FileLoader* loader, FSPath path)
{
    auto content = loader->read<uint8_t>(path);

    if (content.empty()) {
        get_logger()->error("Failed to read texture file: {}", path);
        return nullptr;
    }

    ktxTexture2*   ktx_texture;
    KTX_error_code result = ktxTexture2_CreateFromMemory(content.data(), content.size(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktx_texture);

    if (result != KTX_SUCCESS) {
        get_logger()->error("Failed to create KTX texture from memory: {}", ktxErrorString(result));
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

LYRA_EXPORT auto create() -> AssetLoaderAPI
{
    auto api    = AssetLoaderAPI{};
    api.configure = configure_loader;
    api.load    = load_texture_asset;
    api.unload  = unload_texture_asset;
    api.get_supported_extensions = get_supported_loader_extensions;
    return api;
}
