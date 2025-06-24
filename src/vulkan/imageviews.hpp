#pragma once

#include <vulkan/vulkan.hpp>

#include <iostream>
#include <vector>
#include <stdexcept>

class VulkanImageViews {
    public:
        VulkanImageViews(const VkDevice& device, const std::vector<VkImage>& swapChainImages, const VkFormat& swapChainImageFormat): device(device) {
            createImageViews(swapChainImages, swapChainImageFormat);
        }

        ~VulkanImageViews() {
            for (auto imageView : imageViews) {
                if (imageView != nullptr)
                    vkDestroyImageView(device, imageView, nullptr);
            }
            imageViews.clear();
        }

        const std::vector<VkImageView>& getImageViews() {
            return imageViews;
        }

    private:
        VkDevice device = VK_NULL_HANDLE;
        std::vector<VkImageView> imageViews;

        void createImageViews(const std::vector<VkImage> &swapChainImages, const VkFormat& swapChainImageFormat) {
            imageViews.resize(swapChainImages.size());

            for (size_t i = 0; i < swapChainImages.size(); i++) {
                imageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat);
            }

            if (imageViews.empty()) {
                throw std::runtime_error("Failed to create Image Views");
            }
        }

        VkImageView createImageView(VkImage image, VkFormat format) {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = image;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = format;

            viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            VkImageView imageView;

            if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create image view!");
            }

            return imageView;
        }
};