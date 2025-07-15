#pragma once

#include "vulkan/raster/buffer.hpp"
#include "vulkan/raster/device_memory.hpp"
#include "model.hpp"
#include "texture.hpp"
#include "texture_image.hpp"
#include "sphere.hpp"
#include "vulkan/utils/buffer.hpp"

#include <array>
#include <memory>

// Axis-Aligned Bounding Box -> used for spatial partitioning, collision detection, and ray intersection culling.

// Procedural Geometry -> vertices and indices with mathematical formulas or code
class VulkanSceneResources {
    public:
        VulkanSceneResources(
            const VulkanDevice& device,
            VulkanCommandPool& commandPool, 
            std::vector<VulkanModel>&& models, 
            std::vector<VulkanTexture>&& textures,
            VkQueue graphicsQueue
        ) : 
            models(std::move(models)),
	        textures(std::move(textures))
        {
            aggregateModelData();
            createBuffers(device, commandPool);
            uploadTextures(device, commandPool);
        }

        ~VulkanSceneResources() = default;

        void aggregateModelData() {
            for (const auto& model : models) {
                uint32_t indexOffset = static_cast<uint32_t>(indices.size());
                uint32_t vertexOffset = static_cast<uint32_t>(vertices.size());
                uint32_t materialOffset = static_cast<uint32_t>(materials.size());

                offsets.emplace_back(indexOffset, vertexOffset);

                vertices.insert(vertices.end(), model.getVertices().begin(), model.getVertices().end());
                indices.insert(indices.end(), model.getIndices().begin(), model.getIndices().end());
                materials.insert(materials.end(), model.getMaterials().begin(), model.getMaterials().end());

                // Adjust material indices
                for (size_t i = vertexOffset; i < vertices.size(); ++i) {
                    vertices[i].materialIndex += materialOffset;
                }

                // Optional procedural geometry
                if (auto* sphere = dynamic_cast<const VulkanSphere*>(model.getProcedural())) {
                    auto aabb = sphere->getBoundingBox();
                    aabbs.push_back({
                        // first of the pair returned
                        aabb.first.x, 
                        aabb.first.y, 
                        aabb.first.z,
                        // second of the pair returned
                        aabb.second.x, 
                        aabb.second.y, 
                        aabb.second.z
                    });
                    procedurals.emplace_back(sphere->getCenter(), sphere->getRadius());
                } else {
                    aabbs.emplace_back();
                    procedurals.emplace_back();
                }
            }
        }

        void uploadTextures(const VulkanDevice& device, VulkanCommandPool& commandPool) {
            textureImages.reserve(textures.size());
            textureImageView.reserve(textures.size());
            textureSampler.reserve(textures.size());

            for (const auto& texture : textures) {
                auto textureImage = std::make_unique<VulkanTextureImage>(device.getDevice(), device.getPhysicalDevice(), device.getGraphicsQueue(), commandPool, texture);
                textureImageView.push_back(textureImage->getImageView().getImageView());
                textureSampler.push_back(textureImage->getSampler().getSampler());
                textureImages.push_back(std::move(textureImage));
            }
        }

        void createBuffers(const VulkanDevice& device, VulkanCommandPool& commandPool) {
            constexpr auto flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

            vertexBuffer = utils::createDeviceBuffer(
                device,
                commandPool,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | flags,
                vertices
            );

            indexBuffer = utils::createDeviceBuffer(
                device,
                commandPool,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | flags,
                indices
            );

            materialBuffer = utils::createDeviceBuffer(
                device,
                commandPool,
                flags,
                materials
            );

            offsetBuffer = utils::createDeviceBuffer(
                device,
                commandPool, 
                flags,
                offsets
            );

            aabbBuffer = utils::createDeviceBuffer(
                device,
                commandPool,
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | flags,
                aabbs
            );

            proceduralBuffer = utils::createDeviceBuffer(
                device,
                commandPool,
                flags,
                procedurals
            );
        }

        const std::vector<VulkanModel>& getModels() const {
            return models;
        }

        const std::vector<VulkanTexture> getTextures() const {
            return textures;
        }

        const std::vector<VkImageView>& getTextureImageViews() const { 
            return textureImageView; 
        }
        
	    const std::vector<VkSampler>& getTextureSamplers() const { 
            return textureSampler; 
        }

        const VulkanBuffer& getVertexBuffer() const {
            return *vertexBuffer.buffer;
        }

        const VulkanBuffer& getIndexBuffer() const {
            return *indexBuffer.buffer;
        }

        const VulkanBuffer& getMaterialBuffer() const {
            return *materialBuffer.buffer;
        }

        const VulkanBuffer& getOffsetBuffer() const {
            return *offsetBuffer.buffer;
        }   

        const VulkanBuffer& getAaBbBuffer() const {
            return *aabbBuffer.buffer;
        }

        const VulkanBuffer& getProceduralBuffer() const {
            return *proceduralBuffer.buffer;
        }

        bool isProcedurals() const { 
            return static_cast<bool>(proceduralBuffer.buffer); 
        }

        void clearResources() {
            textureSampler.clear();
            textureImageView.clear();
            textureImages.clear();

            models.clear();
            textures.clear();

            vertices.clear();
            indices.clear();
            materials.clear();
            offsets.clear();
            aabbs.clear();
            procedurals.clear();
        }

    private:
        std::vector<VulkanModel> models;
        std::vector<VulkanTexture> textures;

        // Aggregated GPU data
        std::vector<VulkanVertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<VulkanMaterial> materials;
        std::vector<glm::vec4> procedurals;
        std::vector<VkAabbPositionsKHR> aabbs;
        std::vector<glm::uvec2> offsets;

		std::vector<std::unique_ptr<VulkanTextureImage>> textureImages;
		std::vector<VkImageView> textureImageView;
		std::vector<VkSampler> textureSampler;

        utils::BufferResource vertexBuffer;
        utils::BufferResource indexBuffer;
        utils::BufferResource materialBuffer;
        utils::BufferResource offsetBuffer;
        utils::BufferResource aabbBuffer;
        utils::BufferResource proceduralBuffer;
};
