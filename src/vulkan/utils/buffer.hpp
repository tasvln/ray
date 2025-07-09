#pragma once

#include "vulkan/raster/buffer.hpp"
#include "vulkan/raster/device.hpp"
#include "vulkan/raster/command_pool.hpp"
#include "vulkan/raster/device_memory.hpp"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace utils {
    template <typename T>
    void copyFromStagingBuffer(
        const VkDevice& device,
        VulkanCommandPool& pool, 
        const VulkanBuffer& buffer, 
        const std::vector<T>& content,
        VkQueue graphicsQueue
    ) {
        const auto contentSize = sizeof(content[0]) * content.size();

        auto stagingBuffer = std::make_unique<VulkanBuffer>(device, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, contentSize);
        VulkanDeviceMemory stagingBufferMemory = stagingBuffer->allocateMemory(device.physicalDevice, 0, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        const auto data = stagingBufferMemory.map(0, content.size());
        std::memcpy(data, content.data(), contentSize);
        stagingBufferMemory.unMap();

        buffer.copyFrom(pool, *stagingBuffer, contentSize, graphicsQueue);
    };

    template <typename T>
    struct BufferResource {
        std::unique_ptr<VulkanBuffer> buffer;
        std::unique_ptr<VulkanDeviceMemory> memory;

        // Allow move construction
        BufferResource(BufferResource&& other) noexcept
            : buffer(std::move(other.buffer)), memory(std::move(other.memory)) {}

        // Allow move assignment
        BufferResource& operator=(BufferResource&& other) noexcept {
            if (this != &other) {
                buffer = std::move(other.buffer);
                memory = std::move(other.memory);
            }
            return *this;
        }

        // Delete copy operations (optional but clear)
        BufferResource(const BufferResource&) = delete;
        BufferResource& operator=(const BufferResource&) = delete;

        BufferResource() = default;
    };

    template <typename T>
    BufferResource<T> createDeviceBuffer(
        const VulkanDevice& device,
        VulkanCommandPool& pool,
        VkBufferUsageFlags usage,
        const std::vector<T>& content
    ) {
        const auto contentSize = sizeof(content[0]) * content.size();

        const VkMemoryAllocateFlags allocateFlags = 
            (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
            ? VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
            : 0;

        BufferResource<T> resource;
        resource.buffer = std::make_unique<VulkanBuffer>(device.device, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage, contentSize);
        resource.memory = std::make_unique<VulkanDeviceMemory>(resource.buffer->allocateMemory(device.physicalDevice, allocateFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

        copyFromStagingBuffer(device.device, pool, *resource.buffer, content, device.graphicsQueue);

        return resource;
    }
}