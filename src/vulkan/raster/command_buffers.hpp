#pragma once

#include "command_pool.hpp"

class VulkanCommandBuffers{
    public:
        VulkanCommandBuffers(const VkDevice& device, VulkanCommandPool& commandPool, const uint32_t size) : device(device), pool(commandPool) {
            VkCommandBufferAllocateInfo allocInfo = {};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.commandPool = pool.getPool();
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandBufferCount = size;

            commandBuffers.resize(size);

            if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
                throw std::runtime_error("Failed to allocate command buffers");
            }
        }

        ~VulkanCommandBuffers() {
            if (!commandBuffers.empty())
            {
                vkFreeCommandBuffers(device, pool.getPool(), static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
                commandBuffers.clear();
            }
        }

        VkCommandBuffer begin(const size_t index)
        {
            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
            beginInfo.pInheritanceInfo = nullptr; // Optional

            if (vkBeginCommandBuffer(commandBuffers[index], &beginInfo) != VK_SUCCESS) {
                throw std::runtime_error("Failed to begin command buffer");
            }

            return commandBuffers[index];
        }

        void end(const size_t index) {
            if (vkEndCommandBuffer(commandBuffers[index]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to end the buffer");
            }
        }

        const std::vector<VkCommandBuffer>& getCommandBuffers() {
            return commandBuffers;
        }

    private:
        VkDevice device = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> commandBuffers;
        VulkanCommandPool pool;
};