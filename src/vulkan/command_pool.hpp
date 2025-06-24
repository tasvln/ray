#pragma once

#include <vulkan/vulkan.hpp>

class VulkanCommandPool {
    public:
        VulkanCommandPool(const VkDevice& device, const uint32_t queueFamilyIndex, const bool allowReset): device(device) {
            VkCommandPoolCreateInfo poolInfo = {};
            poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.queueFamilyIndex = queueFamilyIndex;
            poolInfo.flags = allowReset ? VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT : 0;

            if (vkCreateCommandPool(device, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create command pool");
            }
        }

        ~VulkanCommandPool() {
            if (pool != nullptr) {
                vkDestroyCommandPool(device, pool, nullptr);
                pool = nullptr;
            }
        }

        const VkCommandPool getPool() const {
            return pool;
        }

    private:
        VkDevice device = VK_NULL_HANDLE;
        VkCommandPool pool = VK_NULL_HANDLE;
};