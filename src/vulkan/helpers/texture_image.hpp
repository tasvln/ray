#pragma once

#include <memory>
#include <cstring>

#include "buffer.hpp"
#include "command_pool.hpp"
#include "device_memory.hpp"
#include "image_view.hpp"
#include "texture.hpp"
#include "image.hpp"

class VulkanTextureImage{
    public:
        VulkanTextureImage(
            const VkDevice device, 
            const VkQueue graphicsQueue,
            const VkPhysicalDevice& physicalDevice, 
            VulkanCommandPool& commandPool, 
            const VulkanTexture& texture
        ) {
            const VkDeviceSize imageSize = texture.getWidth() * texture.getHeight() * 4;
            
            auto stagingBuffer = std::make_unique<VulkanBuffer>(device, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, imageSize);
            auto stagingMemory = stagingBuffer->allocateMemory(physicalDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            // copy data into staging buffer
            void* data = stagingMemory.map(0, imageSize);
            std::memcpy(data, texture.getPixels(), imageSize);
            stagingMemory.unMap();

            // gpu image
            VkExtent2D extent{
                static_cast<uint32_t>(texture.getWidth()),
                static_cast<uint32_t>(texture.getHeight())
            };

            image = std::make_unique<VulkanImage>(device, physicalDevice, extent, VK_FORMAT_R8G8B8A8_UNORM);
            imageMemory = std::make_unique<VulkanDeviceMemory>(image->allocateMemory());
            imageView = std::make_unique<VulkanImageView>(device, image->getImage(), image->getFormat(), VK_IMAGE_ASPECT_COLOR_BIT);
            sampler = std::make_unique<VulkanSampler>(device, VulkanSamplerConfig());

            image->transitionLayout(commandPool, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, graphicsQueue);
            image->copyFrom(commandPool, *stagingBuffer, graphicsQueue);
            image->transitionLayout(commandPool, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, graphicsQueue);
        }

        VulkanTextureImage(const VulkanTextureImage&) = delete;
        VulkanTextureImage(VulkanTextureImage&&) = delete;
        VulkanTextureImage& operator=(const VulkanTextureImage&) = delete;
        VulkanTextureImage& operator=(VulkanTextureImage&&) = delete;

        ~VulkanTextureImage() = default;

        const VulkanImage& getImage() const {
            return *image;
        }

        const VulkanDeviceMemory& getImageMemory() const {
            return *imageMemory;
        }

        const VulkanImageView& getImageView() const {
            return *imageView;
        }

        const VulkanSampler& getSampler() const {
            return *sampler;
        }


    private:
        std::unique_ptr<VulkanImage> image;
        std::unique_ptr<VulkanDeviceMemory> imageMemory;
        std::unique_ptr<VulkanImageView> imageView;
        std::unique_ptr<VulkanSampler> sampler;
};