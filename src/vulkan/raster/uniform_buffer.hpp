#pragma once

#include "buffer.hpp"
#include <glm/glm.hpp>
#include <array>
#include <memory>

struct UniformData {
    glm::mat4 modelView;
    glm::mat4 modelViewInverse;
    glm::mat4 projection;
    glm::mat4 projectionInverse;

    // glm::vec3 cameraPosition;

    // todo -> what's the point of this
    float aperture;
    float focusDistance;
    float heatmapScale;
    uint32_t totalNumberOfSamples;
    uint32_t numberOfSamples;
    uint32_t numberOfBounces;
    uint32_t randomSeed;
    uint32_t hasSky; // bool
    uint32_t showHeatmap; // bool
};

class VulkanUniformBuffer {
    public:
        VulkanUniformBuffer(const VkDevice& device, const VkPhysicalDevice& physicalDevice): device(device) {
            // const auto bufferSize = sizeof(VulkanUniformBuffer);
            const auto bufferSize = sizeof(UniformData);

            buffer = std::make_unique<VulkanBuffer>(device, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, bufferSize);
            memory = std::make_unique<VulkanDeviceMemory>(buffer->allocateMemory(physicalDevice, 0, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
        }

        VulkanUniformBuffer(VulkanUniformBuffer&& other) noexcept : buffer(std::move(other.buffer)), memory(std::move(other.memory)) {}
        
        VulkanUniformBuffer& operator=(VulkanUniformBuffer&& other) noexcept {
            if (this != &other) {
                buffer = std::move(other.buffer);
                memory = std::move(other.memory);
            }
            return *this;
        }

        ~VulkanUniformBuffer() = default;

        const VulkanBuffer& getBuffer() const {
            return *buffer;
        }

        const VulkanDeviceMemory& getMemory() const {
            return *memory;
        }

        void setValue(const UniformData& ubo) {
            const auto data = memory->map(0, sizeof(UniformData));
            std::memcpy(data, &ubo, sizeof(ubo));
            memory->unMap();
        }
    private: 
        VkDevice device = VK_NULL_HANDLE;
        std::unique_ptr<VulkanBuffer> buffer;
        std::unique_ptr<VulkanDeviceMemory> memory;
};