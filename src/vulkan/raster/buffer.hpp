#pragma once

#include "command_buffers.hpp"
#include "command_pool.hpp"
#include "device_memory.hpp"

class VulkanBuffer{
    public:
        VulkanBuffer(const VkDevice& device, const VkBufferUsageFlags usage, const size_t size) : device(device) {
            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = size;
            bufferInfo.usage = usage;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create buffer!");
            }
        }

        ~VulkanBuffer() {
            if (buffer != nullptr) {
                vkDestroyBuffer(device, buffer, nullptr);
                buffer = nullptr;
            }
        }

        VulkanDeviceMemory allocateMemory(const VkPhysicalDevice& physicalDevice, const VkMemoryAllocateFlags allocateFlags = 0, const VkMemoryPropertyFlags propertyFlags) {
            const auto requirements = getMemoryRequirements();
            VulkanDeviceMemory memory(device, physicalDevice, requirements.memoryTypeBits, allocateFlags, propertyFlags, requirements.size);

            if (vkBindBufferMemory(device, buffer, memory.getMemory(), 0) != VK_SUCCESS) {
                throw std::runtime_error("Failed to bind buffer memory!");
            }

            return memory;
        }

        VkMemoryRequirements getMemoryRequirements() const
        {
            VkMemoryRequirements requirements;
            vkGetBufferMemoryRequirements(device, buffer, &requirements);
            return requirements;
        }

        VkDeviceAddress getDeviceAddress() const {
            VkBufferDeviceAddressInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            info.pNext = nullptr;
            info.buffer = buffer;

            return vkGetBufferDeviceAddress(device, &info);
        }

        void copyFrom(VulkanCommandPool& commandPool, const VulkanBuffer& src, VkDeviceSize size, VkQueue graphicsQueue) {
			VulkanCommandBuffers commandBuffers(device, commandPool, 1);

			VkCommandBufferBeginInfo beginInfo = {};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

            vkBeginCommandBuffer(commandBuffers.getCommandBuffers()[0], &beginInfo);

            VkBufferCopy copyRegion = {};
            copyRegion.srcOffset = 0; // Optional
            copyRegion.dstOffset = 0; // Optional
            copyRegion.size = size;

            vkCmdCopyBuffer(commandBuffers.getCommandBuffers()[0], src.getBuffer(), buffer, 1, &copyRegion);

            vkEndCommandBuffer(commandBuffers.getCommandBuffers()[0]);

			VkSubmitInfo submitInfo = {};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffers.getCommandBuffers()[0];

            vkQueueSubmit(graphicsQueue, 1, &submitInfo, nullptr);
			vkQueueWaitIdle(graphicsQueue);
        }

        const VkBuffer getBuffer() const {
            return buffer;
        }

    private:
        VkDevice device = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
};