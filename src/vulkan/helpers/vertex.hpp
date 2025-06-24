#pragma once

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <array>

class VulkanVertex {
	public:
        glm::vec3 position;
		glm::vec3 normal;
		glm::vec2 texCoord;
		int32_t materialIndex;

        bool operator==(const VulkanVertex& other) const
		{
			return 
				position == other.position &&
				normal == other.normal &&
				texCoord == other.texCoord &&
				materialIndex == other.materialIndex;
		}

		// STATIC -> don't depend on object state
		static constexpr VkVertexInputBindingDescription GetBindingDescription()
		{
			VkVertexInputBindingDescription bindingDescription = {};
			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(VulkanVertex);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			return bindingDescription;
		}

		static constexpr std::array<VkVertexInputAttributeDescription, 4> GetAttributeDescriptions()
		{
			std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions = {};

			attributeDescriptions[0].binding = 0;
			attributeDescriptions[0].location = 0;
			attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[0].offset = offsetof(VulkanVertex, position);

			attributeDescriptions[1].binding = 0;
			attributeDescriptions[1].location = 1;
			attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[1].offset = offsetof(VulkanVertex, normal);

			attributeDescriptions[2].binding = 0;
			attributeDescriptions[2].location = 2;
			attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
			attributeDescriptions[2].offset = offsetof(VulkanVertex, texCoord);

			attributeDescriptions[3].binding = 0;
			attributeDescriptions[3].location = 3;
			attributeDescriptions[3].format = VK_FORMAT_R32_SINT;
			attributeDescriptions[3].offset = offsetof(VulkanVertex, materialIndex);

			return attributeDescriptions;
		}
};