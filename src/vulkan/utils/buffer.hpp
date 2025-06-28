#pragma once

#include "buffer.hpp"
#include "device.hpp"
#include "command_pool.hpp"
#include "device_memory.hpp"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace utils {
    template <typename T>
    void copyFromStagingBuffer(
        const VkDevice& device,
        VulkanCommandPool& pool, 
        VulkanBuffer& buffer, 
        const std::vector<T>& content,
        VkQueue graphicsQueue
    ) {
        const auto contentSize = sizeof(content[0]) * content.size();

        auto stagingBuffer = std::make_unique<VulkanBuffer>(device, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, contentSize);
        VulkanDeviceMemory stagingBufferMemory = stagingBuffer->allocateMemory(device.physicalDevice, 0, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        const auto data = stagingBufferMemory.map(0, content.size());
        std::memcpy(data, content.data(), contentSize);
        stagingBufferMemory.unMap();

        buffer.copyFrom(pool, *buffer, contentSize, graphicsQueue);
    });

    template <typename T>
    struct BufferResource {
        std::unique_ptr<VulkanBuffer> buffer;
        std::unique_ptr<DeviceMemory> memory;
    };

    template <typename T>
    BufferResource<T> createDeviceBuffer(
        const VulkanDevice& device,
        VulkanCommandPool& pool,
        VkBufferUsageFlags usage,
        const std::vector<T>& content,
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
    }
}