#pragma once

#include "vulkan/utils/buffer.hpp"
#include <glm/glm.hpp>

struct UniformBufferObject {
    glm::mat4 modelView;
    glm::mat4 projection;

    // why?
    glm::mat4 modelViewInverse;
    glm::mat4 projectionInverse;

    // 
    float aperture;
    float focusDistance;
    float heatMapScale;

    uint32_t totalNumberOfSamples;
    uint32_t numberOfSamples;
    uint32_t numberOfBounces;

    // why?
    uint32_t randomSeed;

    uint32_t hasSky; // bool
    uint32_t showHeatmap; // bool
};

class VulkanUniformBuffer {
    public:
        VulkanUniformBuffer(const VulkanDevice& device) {
            const auto bufferSize = sizeof(UniformBufferObject);

            uboResource.buffer = std::make_unique<VulkanBuffer>(
                device,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                bufferSize
            );

            uboResource.memory = std::make_unique<VulkanDeviceMemory>(
                uboResource.buffer->allocateMemory(
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                )
            );
        }

        // VulkanUniformBuffer(VulkanUniformBuffer&& other) noexcept : buffer(std::move(other.buffer)), memory(std::move(other.memory)) {}
        
        // VulkanUniformBuffer& operator=(VulkanUniformBuffer&& other) noexcept {
        //     if (this != &other) {
        //         buffer = std::move(other.buffer);
        //         memory = std::move(other.memory);
        //     }
        //     return *this;
        // }

        ~VulkanUniformBuffer() = default;

        const VulkanBuffer& getBuffer() const {
            return *uboResource.buffer;
        }

        const VulkanDeviceMemory& getMemory() const {
            return *uboResource.memory;
        }

        void setUniformBufferInMemory(const UniformBufferObject& ubo) {
            const auto data = uboResource.memory->map(0, sizeof(UniformBufferObject));
            std::memcpy(data, &ubo, sizeof(ubo));
            uboResource.memory->unMap();
        }

    private:
        utils::BufferResource uboResource;
};