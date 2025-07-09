#pragma once

#include <vulkan/vulkan.hpp>

#include "device.hpp"

class VulkanFence {
    public:
        VulkanFence(const VkDevice& device, const bool signaled): device(device){
            VkFenceCreateInfo fenceInfo = {};
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

            if (vkCreateFence(device, &fenceInfo, VK_NULL_HANDLE, &fence) != VK_SUCCESS) {
                throw std::runtime_error("Failed to Create Fence");
            }
        }

        VulkanFence(VulkanFence&& src) noexcept : device(src.device), fence(src.fence)  {
            src.fence = VK_NULL_HANDLE;
        }

        VulkanFence& operator=(VulkanFence&& src) noexcept = delete;

        VulkanFence(const VulkanFence&) = delete;
        VulkanFence& operator=(const VulkanFence&) = delete;

        ~VulkanFence() {
            if (fence != VK_NULL_HANDLE) {
                vkDestroyFence(device, fence, VK_NULL_HANDLE);
            }
        }

        const VkFence getFence() const {
            return fence;
        }

        void reset() {
            if (vkResetFences(device, 1, &fence) != VK_SUCCESS) {
                throw std::runtime_error("Failed to Reset Fence");
            }
        }

        void await(const uint64_t timeout) const {
            if (vkWaitForFences(device, 1, &fence, VK_TRUE, timeout) != VK_SUCCESS) {
                throw std::runtime_error("Failed to Wait for Fence");
            }
        }

    private:
        VkDevice device;
        VkFence fence{VK_NULL_HANDLE};
};