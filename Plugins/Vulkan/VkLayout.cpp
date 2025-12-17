#include "VkUtils.h"

VkDescriptorType infer_buffer_descriptor_type(const GPUBufferBindingLayout& entry)
{
    switch (entry.type) {
        case GPUBufferBindingType::STORAGE:
        case GPUBufferBindingType::READ_ONLY_STORAGE:
            return entry.has_dynamic_offset ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case GPUBufferBindingType::UNIFORM:
            return entry.has_dynamic_offset ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        default:
            throw std::invalid_argument("Invalid GPUBufferBindingType!");
    }
}

VkDescriptorType infer_sampler_descriptor_type(const GPUSamplerBindingLayout&)
{
    return VK_DESCRIPTOR_TYPE_SAMPLER;
}

VkDescriptorType infer_texture_descriptor_type(const GPUTextureBindingLayout&)
{
    return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
}

VkDescriptorType infer_storage_texture_descriptor_type(const GPUStorageTextureBindingLayout&)
{
    return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
}

VkDescriptorType infer_bvh_descriptor_type(const GPUBVHBindingLayout&)
{
    return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
}

VkDescriptorType infer_descriptor_type(const GPUBindGroupLayoutEntry& entry)
{
    switch (entry.type) {
        case GPUBindingResourceType::BUFFER:
            return infer_buffer_descriptor_type(entry.buffer);
        case GPUBindingResourceType::SAMPLER:
            return infer_sampler_descriptor_type(entry.sampler);
        case GPUBindingResourceType::TEXTURE:
            return infer_texture_descriptor_type(entry.texture);
        case GPUBindingResourceType::STORAGE_TEXTURE:
            return infer_storage_texture_descriptor_type(entry.storage_texture);
        case GPUBindingResourceType::ACCELERATION_STRUCTURE:
            return infer_bvh_descriptor_type(entry.bvh);
        default:
            throw std::invalid_argument("Unsupported GPU binding resource type!");
    }
}

// NOTE: A possible optimization is that we could hash the layout
// to avoid creating identical layouts, not sure if this is useful.

VulkanBindGroupLayout::VulkanBindGroupLayout() : layout(VK_NULL_HANDLE)
{
    // do nothing
}

VulkanBindGroupLayout::VulkanBindGroupLayout(const GPUBindGroupLayoutDescriptor& desc)
{
    VkDescriptorBindingFlags flags[] = {VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT};

    auto bindingflags_info          = VkDescriptorSetLayoutBindingFlagsCreateInfo{};
    bindingflags_info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingflags_info.bindingCount  = 0;
    bindingflags_info.pBindingFlags = nullptr;

    // extract binding information for the descriptor set
    binding_types.clear();
    Vector<VkDescriptorSetLayoutBinding> bindings;
    for (auto& entry : desc.entries) {
        auto type                  = infer_descriptor_type(entry);
        auto binding               = VkDescriptorSetLayoutBinding{};
        binding.binding            = entry.binding.index;
        binding.descriptorCount    = entry.count;
        binding.descriptorType     = type;
        binding.stageFlags         = vkenum(entry.visibility);
        binding.pImmutableSamplers = nullptr;
        bindings.push_back(binding);

        // keep track of basic properties for bind group layout
        binding_types.push_back(binding.descriptorType);

        // variable size descriptor binding
        if (bindless)
            bindingflags_info.pBindingFlags = flags;
    }

    layout = VK_NULL_HANDLE;
    // bindless = desc.bindless;

    if (!bindings.empty()) {
        // prepare create info
        auto create_info         = VkDescriptorSetLayoutCreateInfo{};
        create_info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        create_info.pBindings    = bindings.data();
        create_info.bindingCount = static_cast<uint32_t>(bindings.size());
        create_info.pNext        = &bindingflags_info;

        // create descritpor set layout
        auto rhi = get_rhi();
        vk_check(rhi->vtable.vkCreateDescriptorSetLayout(rhi->device, &create_info, nullptr, &layout));

        if (desc.label)
            rhi->set_debug_label(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)layout, desc.label);
    }
}

void VulkanBindGroupLayout::destroy()
{
    auto rhi = get_rhi();
    rhi->vtable.vkDestroyDescriptorSetLayout(rhi->device, layout, nullptr);
    layout = VK_NULL_HANDLE;
}

VulkanPipelineLayout::VulkanPipelineLayout() : layout(VK_NULL_HANDLE)
{
    // do nothing
}

VulkanPipelineLayout::VulkanPipelineLayout(const GPUPipelineLayoutDescriptor& desc)
{
    auto rhi = get_rhi();

    Vector<VkDescriptorSetLayout> bind_group_layouts;
    for (const auto& handle : desc.bind_group_layouts) {
        auto& bind_group_layout = rhi->bind_group_layouts.at(handle.value);
        assert(bind_group_layout.layout != VK_NULL_HANDLE);
        bind_group_layouts.push_back(bind_group_layout.layout);
    }

    Vector<VkPushConstantRange> push_constant_ranges;
    for (const auto& range : desc.push_constant_ranges) {
        push_constant_ranges.push_back({});
        auto& push_constant      = push_constant_ranges.back();
        push_constant.size       = range.size;
        push_constant.offset     = range.offset;
        push_constant.stageFlags = vkenum(range.visibility);
    }

    // prepare pipeline layout
    auto create_info                   = VkPipelineLayoutCreateInfo{};
    create_info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    create_info.pSetLayouts            = bind_group_layouts.data();
    create_info.setLayoutCount         = static_cast<uint32_t>(bind_group_layouts.size());
    create_info.pPushConstantRanges    = push_constant_ranges.data();
    create_info.pushConstantRangeCount = static_cast<uint32_t>(push_constant_ranges.size());

    // create pipeline layout
    vk_check(rhi->vtable.vkCreatePipelineLayout(rhi->device, &create_info, nullptr, &layout));

    if (desc.label)
        rhi->set_debug_label(VK_OBJECT_TYPE_PIPELINE_LAYOUT, (uint64_t)layout, desc.label);
}

void VulkanPipelineLayout::destroy()
{
    auto rhi = get_rhi();
    rhi->vtable.vkDestroyPipelineLayout(rhi->device, layout, nullptr);
    layout = VK_NULL_HANDLE;
}
