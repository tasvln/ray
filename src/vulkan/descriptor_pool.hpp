#pragma once

#include "descriptorset_layout.hpp"

#include <iostream>
#include <vector>
#include <stdexcept>

class VulkanDescriptorPool {
    public:
        VulkanDescriptorPool(const VkDevice& device, const std::vector<DescriptorBinding>& descriptorBindings, const size_t maxSets): device(device) {
            std::vector<VkDescriptorPoolSize> poolSizes;

            for (const auto& binding : descriptorBindings)
            {
                poolSizes.push_back(VkDescriptorPoolSize{ binding.descriptorType, static_cast<uint32_t>(binding.descriptorCount*maxSets )});
            }
            
            VkDescriptorPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
            poolInfo.pPoolSizes = poolSizes.data();
            poolInfo.maxSets = static_cast<uint32_t>(maxSets);

            
            if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create fullscreen descriptor pool!");
            }
        }

        ~VulkanDescriptorPool() {
            if (pool != nullptr) {
                vkDestroyDescriptorPool(device, pool, nullptr);
                pool = nullptr;
            }
        }

        const VkDescriptorPool& getPool() const {
            return pool;
        }

    private:
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorPool pool = VK_NULL_HANDLE;
};