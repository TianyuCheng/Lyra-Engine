#include "VkUtils.h"

VulkanTexture::VulkanTexture()
    : image(VK_NULL_HANDLE), alloc_info({}), aspects(0)
{
    // do nothing
}

VulkanTexture::VulkanTexture(const GPUTextureDescriptor& desc) : aspects(0)
{
    auto rhi = get_rhi();

    auto tex_create_info   = VkImageCreateInfo{};
    auto alloc_create_info = VmaAllocationCreateInfo{};

    // determine image type
    VkImageType image_type;
    if (desc.size.depth > 1) {
        image_type = VK_IMAGE_TYPE_3D;
    } else if (desc.size.height > 1) {
        image_type = VK_IMAGE_TYPE_2D;
    } else {
        image_type = VK_IMAGE_TYPE_1D;
    }

    // buffer create info
    tex_create_info.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    tex_create_info.extent        = VkExtent3D{desc.size.width, desc.size.height, desc.size.depth};
    tex_create_info.mipLevels     = desc.mip_level_count;
    tex_create_info.arrayLayers   = desc.array_layers;
    tex_create_info.imageType     = image_type;
    tex_create_info.format        = vkenum(desc.format);
    tex_create_info.tiling        = VK_IMAGE_TILING_OPTIMAL;
    tex_create_info.samples       = vkenum(desc.sample_count);
    tex_create_info.usage         = vkenum(desc.usage, desc.format);
    tex_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // allocation create info
    alloc_create_info.usage         = VMA_MEMORY_USAGE_AUTO;
    alloc_create_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    alloc_create_info.flags         = 0;

    // create texture
    vk_check(vmaCreateImage(rhi->alloc,
        &tex_create_info, &alloc_create_info,
        &image, &allocation, &alloc_info));

    // initialize aspects
    if (is_depth_format(desc.format))
        aspects |= VK_IMAGE_ASPECT_DEPTH_BIT;
    if (is_stencil_format(desc.format))
        aspects |= VK_IMAGE_ASPECT_STENCIL_BIT;
    if (aspects == 0)
        aspects |= VK_IMAGE_ASPECT_COLOR_BIT;

    if (desc.label)
        rhi->set_debug_label(VK_OBJECT_TYPE_IMAGE, (uint64_t)image, desc.label);

    // record the format
    format = tex_create_info.format;

    // record the render area
    area.width  = desc.size.width;
    area.height = desc.size.height;
}

void VulkanTexture::destroy()
{
    // only delete the image and its memory when it is NOT externally managed.
    if (image != VK_NULL_HANDLE && alloc_info.size != 0) {
        vmaDestroyImage(get_rhi()->alloc, image, allocation);
        image = VK_NULL_HANDLE;
    }
}

VulkanTextureView::VulkanTextureView()
    : view(VK_NULL_HANDLE)
{
    // do nothing
}

VulkanTextureView::VulkanTextureView(const VulkanTexture& texture, const GPUTextureViewDescriptor& desc)
{
    auto rhi = get_rhi();

    auto create_info = VkImageViewCreateInfo{};

    create_info.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    create_info.image                           = texture.image;
    create_info.viewType                        = vkenum(desc.dimension);
    create_info.format                          = vkenum(desc.format);
    create_info.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.subresourceRange.aspectMask     = vkenum(desc.aspect);
    create_info.subresourceRange.baseMipLevel   = desc.base_mip_level;
    create_info.subresourceRange.levelCount     = desc.mip_level_count;
    create_info.subresourceRange.baseArrayLayer = desc.base_array_layer;
    create_info.subresourceRange.layerCount     = desc.array_layer_count;

    vk_check(vkCreateImageView(rhi->device, &create_info, nullptr, &view));

    if (desc.label)
        rhi->set_debug_label(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)view, desc.label);

    // record the render area
    area = texture.area;

    // record the aspect flags
    aspects = texture.aspects;
}

void VulkanTextureView::destroy()
{
    auto rhi = get_rhi();
    rhi->vtable.vkDestroyImageView(rhi->device, view, nullptr);
    view = VK_NULL_HANDLE;
}
