#include <filesystem>

#include <ktx.h>
#include <vulkan/vulkan.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYEXR_IMPLEMENTATION
#include <tinyexr.h>

#include <gli/gli.hpp>
#include <gli/load.hpp>

#include <Lyra/Common/Macros.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/Assets/Assets.h>
#include <Lyra/Render/RHIAPI.h>

using namespace lyra;

static Logger logger = create_logger("Texture", LogLevel::trace);

Logger get_logger() { return logger; }

// clang-format off
static GPUTextureFormat to_gpu_texture_format(VkFormat format)
{
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
        default:                             return GPUTextureFormat::RGBA8UNORM;
    }
}

// helper to map gli format to vkFormat
static VkFormat gli_to_vk_format(gli::format format)
{
    switch (format) {
        case gli::FORMAT_RGBA8_UNORM_PACK8:       return VK_FORMAT_R8G8B8A8_UNORM;
        case gli::FORMAT_RGBA8_SRGB_PACK8:        return VK_FORMAT_R8G8B8A8_SRGB;
        case gli::FORMAT_BGRA8_UNORM_PACK8:       return VK_FORMAT_B8G8R8A8_UNORM;
        case gli::FORMAT_BGRA8_SRGB_PACK8:        return VK_FORMAT_B8G8R8A8_SRGB;
        case gli::FORMAT_RGBA32_SFLOAT_PACK32:    return VK_FORMAT_R32G32B32A32_SFLOAT;
        case gli::FORMAT_RGBA_DXT1_UNORM_BLOCK8:  return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        case gli::FORMAT_RGBA_DXT1_SRGB_BLOCK8:   return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
        case gli::FORMAT_RGBA_DXT3_UNORM_BLOCK16: return VK_FORMAT_BC2_UNORM_BLOCK;
        case gli::FORMAT_RGBA_DXT3_SRGB_BLOCK16:  return VK_FORMAT_BC2_SRGB_BLOCK;
        case gli::FORMAT_RGBA_DXT5_UNORM_BLOCK16: return VK_FORMAT_BC3_UNORM_BLOCK;
        case gli::FORMAT_RGBA_DXT5_SRGB_BLOCK16:  return VK_FORMAT_BC3_SRGB_BLOCK;
        case gli::FORMAT_R_ATI1N_UNORM_BLOCK8:    return VK_FORMAT_BC4_UNORM_BLOCK;
        case gli::FORMAT_R_ATI1N_SNORM_BLOCK8:    return VK_FORMAT_BC4_SNORM_BLOCK;
        case gli::FORMAT_RG_ATI2N_UNORM_BLOCK16:  return VK_FORMAT_BC5_UNORM_BLOCK;
        case gli::FORMAT_RG_ATI2N_SNORM_BLOCK16:  return VK_FORMAT_BC5_SNORM_BLOCK;
        case gli::FORMAT_RGB_BP_UFLOAT_BLOCK16:   return VK_FORMAT_BC6H_UFLOAT_BLOCK;
        case gli::FORMAT_RGB_BP_SFLOAT_BLOCK16:   return VK_FORMAT_BC6H_SFLOAT_BLOCK;
        case gli::FORMAT_RGBA_BP_UNORM_BLOCK16:   return VK_FORMAT_BC7_UNORM_BLOCK;
        case gli::FORMAT_RGBA_BP_SRGB_BLOCK16:    return VK_FORMAT_BC7_SRGB_BLOCK;
        default:                                  return VK_FORMAT_UNDEFINED;
    }
}

// clang-format on

static void* load_texture_asset(AssetServer*, FileLoader* loader, const JSON& metadata)
{
    auto path    = metadata["path"].template get<String>();
    auto content = loader->read<uint8_t>(path.c_str());

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

static void unload_texture_asset(AssetServer*, void* asset)
{
    delete reinterpret_cast<TextureAsset*>(asset);
}

static JSON save_to_ktx2(ktxTexture2* texture, OSPath target_path)
{
    KTX_error_code result = ktxTexture_WriteToNamedFile(ktxTexture(texture), reinterpret_cast<const char*>(target_path));
    ktxTexture_Destroy(ktxTexture(texture));

    if (result != KTX_SUCCESS) {
        get_logger()->error("Failed to write KTX2 texture to file: {}", ktxErrorString(result));
        return {};
    }

    auto metadata    = JSON{};
    metadata["path"] = reinterpret_cast<const char*>(target_path);
    return metadata;
}

static JSON encode_and_save_simple(void* pixels, int width, int height, size_t pixel_size, VkFormat format, OSPath target_path)
{
    ktxTexture2*         texture;
    ktxTextureCreateInfo create_info;
    create_info.glInternalformat = 0;
    create_info.vkFormat         = format;
    create_info.baseWidth        = width;
    create_info.baseHeight       = height;
    create_info.baseDepth        = 1;
    create_info.numDimensions    = 2;
    create_info.numLevels        = 1;
    create_info.numLayers        = 1;
    create_info.numFaces         = 1;
    create_info.isArray          = KTX_FALSE;
    create_info.generateMipmaps  = KTX_TRUE;

    KTX_error_code result = ktxTexture2_Create(&create_info, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture);
    if (result != KTX_SUCCESS) {
        get_logger()->error("Failed to create KTX2 texture: {}", ktxErrorString(result));
        return {};
    }

    result = ktxTexture_SetImageFromMemory(ktxTexture(texture), 0, 0, 0, static_cast<ktx_uint8_t*>(pixels), width * height * 4 * pixel_size);
    if (result != KTX_SUCCESS) {
        get_logger()->error("Failed to set image data for KTX2 texture: {}", ktxErrorString(result));
        ktxTexture_Destroy(ktxTexture(texture));
        return {};
    }

    return save_to_ktx2(texture, target_path);
}

static JSON process_exr(const String& source_path, OSPath target_path)
{
    int         width, height;
    float*      pixels = nullptr;
    const char* err    = nullptr;
    int         ret    = LoadEXR(&pixels, &width, &height, source_path.c_str(), &err);
    if (ret != TINYEXR_SUCCESS) {
        get_logger()->error("Failed to load EXR file: {} (error: {})", source_path, err ? err : "unknown");
        FreeEXRErrorMessage(err);
        return {};
    }

    auto metadata = encode_and_save_simple(pixels, width, height, sizeof(float), VK_FORMAT_R32G32B32A32_SFLOAT, target_path);
    free(pixels); // TinyEXR LoadEXR documentation says use free()
    return metadata;
}

static JSON process_dds(const String& source_path, OSPath target_path)
{
    gli::texture tex = gli::load(source_path);
    if (tex.empty()) {
        get_logger()->error("Failed to load DDS file: {}", source_path);
        return {};
    }

    VkFormat vk_format = gli_to_vk_format(tex.format());
    if (vk_format == VK_FORMAT_UNDEFINED) {
        get_logger()->error("Unsupported DDS format in file: {}", source_path);
        return {};
    }

    ktxTexture2*         ktx_tex;
    ktxTextureCreateInfo create_info;
    create_info.glInternalformat = 0;
    create_info.vkFormat         = vk_format;
    create_info.baseWidth        = tex.extent().x;
    create_info.baseHeight       = tex.extent().y;
    create_info.baseDepth        = tex.extent().z;
    create_info.numDimensions    = tex.target() == gli::TARGET_1D ? 1 : (tex.target() == gli::TARGET_3D ? 3 : 2);
    create_info.numLevels        = static_cast<uint32_t>(tex.levels());
    create_info.numLayers        = static_cast<uint32_t>(tex.layers());
    create_info.numFaces         = static_cast<uint32_t>(tex.faces());
    create_info.isArray          = tex.layers() > 1 ? KTX_TRUE : KTX_FALSE;
    create_info.generateMipmaps  = KTX_FALSE;

    KTX_error_code result = ktxTexture2_Create(&create_info, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &ktx_tex);
    if (result != KTX_SUCCESS) {
        get_logger()->error("Failed to create KTX2 texture from DDS: {}", ktxErrorString(result));
        return {};
    }

    for (size_t layer = 0; layer < tex.layers(); ++layer) {
        for (size_t face = 0; face < tex.faces(); ++face) {
            for (size_t level = 0; level < tex.levels(); ++level) {
                ktxTexture_SetImageFromMemory(ktxTexture(ktx_tex), static_cast<uint32_t>(level), static_cast<uint32_t>(layer), static_cast<uint32_t>(face),
                    static_cast<const ktx_uint8_t*>(tex.data(layer, face, level)), tex.size(level));
            }
        }
    }

    return save_to_ktx2(ktx_tex, target_path);
}

static JSON process_stb(const String& source_path, OSPath target_path)
{
    int      width, height, channels;
    bool     is_hdr     = stbi_is_hdr(source_path.c_str());
    void*    pixels     = nullptr;
    size_t   pixel_size = 0;
    VkFormat format     = VK_FORMAT_UNDEFINED;

    if (is_hdr) {
        pixels     = stbi_loadf(source_path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        pixel_size = sizeof(float);
        format     = VK_FORMAT_R32G32B32A32_SFLOAT;
    } else {
        pixels     = stbi_load(source_path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        pixel_size = sizeof(stbi_uc);
        format     = VK_FORMAT_R8G8B8A8_SRGB;
    }

    if (!pixels) {
        get_logger()->error("Failed to load image file via STB: {}", source_path);
        return {};
    }

    auto metadata = encode_and_save_simple(pixels, width, height, pixel_size, format, target_path);
    stbi_image_free(pixels);
    return metadata;
}

static JSON process_texture_asset(AssetServer* manager, OSPath source_path, OSPath target_path)
{
    auto source_path_str = String(reinterpret_cast<const char*>(source_path));
    auto source_ext      = std::filesystem::path(source_path_str).extension().string();

    // if it's already a ktx file, just copy it
    if (source_ext == ".ktx" || source_ext == ".ktx2") {
        std::filesystem::copy_file(source_path_str, reinterpret_cast<const char*>(target_path), std::filesystem::copy_options::overwrite_existing);
        auto metadata    = JSON{};
        metadata["path"] = reinterpret_cast<const char*>(target_path);
        return metadata;
    }

    if (source_ext == ".exr") {
        return process_exr(source_path_str, target_path);
    } else if (source_ext == ".dds") {
        return process_dds(source_path_str, target_path);
    } else {
        return process_stb(source_path_str, target_path);
    }
}

LYRA_EXPORT auto prepare() -> void
{
    get_logger()->set_level(parse_log_level_from_env("LYRA_TEXTURE_VERBOSITY"));
}

LYRA_EXPORT auto cleanup() -> void
{
    // do nothing
}

LYRA_EXPORT auto create() -> AssetHandlerAPI
{
    auto api    = AssetHandlerAPI{};
    api.load    = load_texture_asset;
    api.unload  = unload_texture_asset;
    api.process = process_texture_asset;
    return api;
}
