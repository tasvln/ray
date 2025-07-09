#pragma once

#include <vulkan/vulkan.hpp>

#include "device.hpp"

class VulkanSemaphore {
    public:
        VulkanSemaphore(const VkDevice& device) : device(device) {
            VkSemaphoreCreateInfo semaphoreInfo = {};
            semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

            if (vkCreateSemaphore(device, &semaphoreInfo, VK_NULL_HANDLE, &semaphore) != VK_SUCCESS) {
                throw std::runtime_error("Failed to Create Semaphore");
            }
        }

        VulkanSemaphore(VulkanSemaphore&& src) noexcept : device(src.device), semaphore(src.semaphore) {
            src.semaphore = VK_NULL_HANDLE;
        }
        VulkanSemaphore& operator=(const VulkanSemaphore&&) = delete;

        VulkanSemaphore(const VulkanSemaphore&) = delete;
        VulkanSemaphore& operator=(const VulkanSemaphore&) = delete;

        ~VulkanSemaphore() {
            if (semaphore != VK_NULL_HANDLE) {
                vkDestroySemaphore(device, semaphore, VK_NULL_HANDLE);
            }
        }

        const VkSemaphore getSemaphore() const {
            return semaphore;
        }

    private:
        VkDevice device;
        VkSemaphore semaphore{VK_NULL_HANDLE};
};