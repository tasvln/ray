#pragma once

#include <vulkan/vulkan.hpp>
#include "device.hpp"

class VulkanDeviceMemory {
    public:
        VulkanDeviceMemory(const VulkanDevice& device, const uint32_t memoryTypeBits, const VkMemoryAllocateFlags allocateFLags, const VkMemoryPropertyFlags propertyFlags, const size_t size) : device(device) {
            VkMemoryAllocateFlagsInfo allocFlagsInfo{};
            allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
            allocFlagsInfo.flags = allocateFLags;
            allocFlagsInfo.pNext = nullptr;

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = size;
            allocInfo.memoryTypeIndex = findMemoryType(device.getPhysicalDevice(), memoryTypeBits, propertyFlags);
            allocInfo.pNext = &allocFlagsInfo;

            if (vkAllocateMemory(device.getDevice(), &allocInfo, nullptr, &memory) != VK_SUCCESS) {
                throw std::runtime_error("Failed to allocate buffer memory!");
            }
        }

        VulkanDeviceMemory(VulkanDeviceMemory&& other) noexcept : device(other.device), memory(other.memory) {
            other.memory = nullptr;
        }

        ~VulkanDeviceMemory() {
            if (memory != nullptr) {
                vkFreeMemory(device.getDevice(), memory, nullptr);
                memory = nullptr;
            }
        }

        void* map(const size_t offset, const size_t size) {
            void* mappedData;
            vkMapMemory(device.getDevice(), memory, offset, size, 0, &mappedData);
            return mappedData;
        }

        void unMap() {
            vkUnmapMemory(device.getDevice(), memory);
        }

        const VkDeviceMemory getMemory() const {
            return memory;
        }

    private:
        VulkanDevice device;
        VkDeviceMemory memory = VK_NULL_HANDLE;

        uint32_t findMemoryType(const VkPhysicalDevice& physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
            VkPhysicalDeviceMemoryProperties memProperties;
            vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

            for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
                if ((typeFilter & (1 << i)) &&
                    (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                    return i;
                }
            }

            throw std::runtime_error("Failed to find suitable memory type!");
        }
};