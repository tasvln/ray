#pragma once

#include "buffer.hpp"
#include "device_memory.hpp"
#include "model.hpp"
#include "texture.hpp"

#include <array>
#include <memory>

struct BufferResource {
	std::unique_ptr<VulkanBuffer> buffer;
	std::unique_ptr<VulkanDeviceMemory> memory;
};

// Axis-Aligned Bounding Box -> used for spatial partitioning, collision detection, and ray intersection culling.

// Procedural Geometry -> vertices and indices with mathematical formulas or code
class VulkanSceneResources {
    public:
        VulkanSceneResources() {
            
        }

        ~VulkanSceneResources() = default;

        const VulkanBuffer& getVertexBuffer() {

        }
        
        const VulkanBuffer& getIndexBuffer() {

        }

        const VulkanBuffer& getMaterialBuffer() {

        }

        const VulkanBuffer& getOffsetBuffer() {

        }

        const VulkanBuffer& getABBuffer() {

        }

        const VulkanBuffer& getProceduralBuffer() {

        }

    private:
        std::unique_ptr<VulkanModel> models;
        std::unique_ptr<VulkanTexture> textures;

		std::vector<std::unique_ptr<VulkanTextureImage>> textureImages;

		std::vector<VkImageView> textureImageViewHandles;
		std::vector<VkSampler> textureSamplerHandles;

        BufferResource vertexBuffer;
        BufferResource indexBuffer;
        BufferResource materialBuffer;
        BufferResource offsetBuffer;
        BufferResource abBuffer;
        BufferResource proceduralBuffer;
        
};
