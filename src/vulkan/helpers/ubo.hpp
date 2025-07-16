#pragma once

#include <glm/glm.hpp>
#include "vulkan/utils/buffer.hpp"

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

		uint32_t isSky; // bool
		uint32_t isHeatmap; // bool
};

class VulkanUBO {
    public:
        VulkanUBO(const VulkanDevice& device) {
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

        ~VulkanUBO() = default;

        void setUBOInMemory(const UniformBufferObject& ubo) {
            const auto data = uboResource.memory->map(0, sizeof(UniformBufferObject));
            std::memcpy(data, &ubo, sizeof(ubo));
            uboResource.memory->unMap();
        }

        const VulkanBuffer& getUBOBuffer() const {
            return *uboResource.buffer;
        }

        const VulkanDeviceMemory& getUBOBufferMemory() const {
            return *uboResource.memory;
        }


    private:
        utils::BufferResource uboResource;
};