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

        VulkanPipelineLayout(const VulkanPipelineLayout&) = delete;
        VulkanPipelineLayout& operator=(const VulkanPipelineLayout&) = delete;

        // Enable move constructor
        VulkanPipelineLayout(VulkanPipelineLayout&& other) noexcept
            : device(other.device), layout(other.layout) {
            other.layout = VK_NULL_HANDLE;
            other.device = VK_NULL_HANDLE;
        }

        // Enable move assignment
        VulkanPipelineLayout& operator=(VulkanPipelineLayout&& other) noexcept {
            if (this != &other) {
                // Destroy current
                if (layout != VK_NULL_HANDLE) {
                    vkDestroyPipelineLayout(device, layout, nullptr);
                }

                // Move
                device = other.device;
                layout = other.layout;

                other.layout = VK_NULL_HANDLE;
                other.device = VK_NULL_HANDLE;
            }
            return *this;
        }

        ~VulkanPipelineLayout() {
            if (layout != nullptr)
            {
                vkDestroyPipelineLayout(device, layout, nullptr);
                layout = nullptr;
            }
        }

        VkPipelineLayout getPipelineLayout() const {
            return layout;
        }

    private:
        VkDevice device = VK_NULL_HANDLE;
        VkPipelineLayout layout = VK_NULL_HANDLE;
};