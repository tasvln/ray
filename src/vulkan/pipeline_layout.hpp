#pragma once

#include "descriptorset_layout.hpp"

#include <iostream>
#include <vector>
#include <stdexcept>

class VulkanPipelineLayout{
    public:
        VulkanPipelineLayout(const VkDevice& device, const VulkanDescriptorSetLayout& descriptorSetLayout) : device(device) {
            VkDescriptorSetLayout descriptorSetLayouts[] = { descriptorSetLayout.getLayout() };

            VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = 1;
            pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts;

            
            if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &layout) != VK_SUCCESS)
                throw std::runtime_error("Failed to create pipeline layout!");
        }

        ~VulkanPipelineLayout() {
            if (layout != nullptr)
            {
                vkDestroyPipelineLayout(device, layout, nullptr);
                layout = nullptr;
            }
        }

    private:
        VkDevice device = VK_NULL_HANDLE;
        VkPipelineLayout layout = VK_NULL_HANDLE;
};