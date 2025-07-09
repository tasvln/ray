#pragma once

#include "image.hpp"
#include "image_view.hpp"
#include "device_memory.hpp"
#include "device.hpp"
#include "command_pool.hpp"

class VulkanDepthBuffer {
    public:
        VulkanDepthBuffer(
            const VulkanDevice& device,
            VulkanCommandPool& commandPool, 
            VkExtent2D extent
        ): 
            format(findDepthFormat(device.getPhysicalDevice())) 
        {
            image = std::make_unique<VulkanImage>(device, extent, format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
            memory = std::make_unique<VulkanDeviceMemory>(image->allocateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
            imageView = std::make_unique<VulkanImageView>(device.getDevice(), image->getImage(), VK_IMAGE_ASPECT_DEPTH_BIT);

            image->transitionLayout(commandPool, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        }

        ~VulkanDepthBuffer() = default;

        static bool hasStencilComponent(const VkFormat format)
		{
			return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
		}

        const VkFormat getFormat() const {
            return format;
        }

        VulkanImage& getImage() const {
            return *image;
        }

        VulkanDeviceMemory& getMemory() const {
            return *memory;
        }

        VulkanImageView& getImageView() const {
            return *imageView;
        }

    private:
        VkFormat format;

        // TODO: put in a struct
        std::unique_ptr<VulkanImage> image;
		std::unique_ptr<VulkanDeviceMemory> memory;
        std::unique_ptr<VulkanImageView> imageView;

        static VkFormat findSupportedFormat(const VkPhysicalDevice& physicalDevice, const std::vector<VkFormat>& candidates, const VkImageTiling tiling, const VkFormatFeatureFlags features) {
            for (VkFormat format : candidates) {
                VkFormatProperties props;
				vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

				if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
					return format;

				if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
					return format;
            }

            throw std::runtime_error("Failed to find supported format for depth buffer -> VulkanDepthBuffer");
        }

        static VkFormat findDepthFormat(const VkPhysicalDevice& physicalDevice) {
            return findSupportedFormat(
                physicalDevice, 
                { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT }, 
                VK_IMAGE_TILING_OPTIMAL, 
                VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
            );
        }
};