#pragma once

#include <vulkan/vulkan.hpp>

#include <iostream>
#include <vector>
#include <stdexcept>

// VkDescriptorSetLayoutBinding samplerBinding{};
// samplerBinding.binding = 0;
// samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
// samplerBinding.descriptorCount = 1;
// samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

struct DescriptorBinding
{
    uint32_t binding;
    uint32_t descriptorCount;
    VkDescriptorType descriptorType;
    VkShaderStageFlags stageFlags; 
};

class VulkanDescriptorSetLayout{
    public:
        VulkanDescriptorSetLayout(const VkDevice& device, const std::vector<DescriptorBinding>& descriptorBindings): device(device) {
            std::vector<VkDescriptorSetLayoutBinding> layoutBindings;

            for (const auto& binding : descriptorBindings)
            {
                VkDescriptorSetLayoutBinding bind = {};
                bind.binding = binding.binding;
                bind.descriptorCount = binding.descriptorCount;
                bind.descriptorType = binding.descriptorType;
                bind.stageFlags = binding.stageFlags;

                layoutBindings.push_back(bind);
            }

            VkDescriptorSetLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
            layoutInfo.pBindings = layoutBindings.data();

            if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &layout) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create fullscreen descriptor set layout!");
            }
        }

        ~VulkanDescriptorSetLayout() {
            if (layout != nullptr)
            {
                vkDestroyDescriptorSetLayout(device, layout, nullptr);
                layout = nullptr;
            }
        }

        const VkDescriptorSetLayout& getLayout() const {
            return layout;
        }

    private:
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
};