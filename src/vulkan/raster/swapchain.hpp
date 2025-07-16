#pragma once

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <algorithm>

#include "core/window.hpp"
#include "device.hpp"
#include "image_view.hpp"

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class VulkanSwapChain{
    public:
        VulkanSwapChain(GLFWwindow* window, const VulkanDevice& device, const VkSurfaceKHR& surface, const VkPresentModeKHR presentMode): device(device) {
            createSwapChain(window, device, surface, presentMode);
        }

        ~VulkanSwapChain() {
            swapChainImageViews.clear();

            if (swapchain != nullptr) {
                vkDestroySwapchainKHR(device.getDevice(), swapchain, nullptr);
                swapchain = nullptr;
            }
        }

        const VkSwapchainKHR& getSwapChain() const {
            return swapchain;
        }

        const VkFormat& getSwapChainFormat() const {
            return swapChainImageFormat;
        }

        const VkExtent2D& getSwapChainExtent() const {
            return swapChainExtent;
        }

        const std::vector<VkImage>& getSwapChainImages() const {
            return swapChainImages;
        }

        const std::vector<std::unique_ptr<VulkanImageView>>& getSwapChainImageViews() const {
            return swapChainImageViews;
        }

    private:
        const VulkanDevice device;
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        VkFormat swapChainImageFormat = VK_FORMAT_UNDEFINED;
        VkExtent2D swapChainExtent = {};

        std::vector<VkImage> swapChainImages;
        std::vector<std::unique_ptr<VulkanImageView>> swapChainImageViews; 

        void createSwapChain(GLFWwindow* window, VulkanDevice device, VkSurfaceKHR surface, VkPresentModeKHR preMode) {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device.getPhysicalDevice(), surface);

            if (swapChainSupport.formats.empty() || swapChainSupport.presentModes.empty()){
                throw std::runtime_error("empty swap chain support");
            }

            VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
            VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes, preMode);
            VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities, window);
            uint32_t imageCount =  chooseImageCount(swapChainSupport.capabilities);

            VkSwapchainCreateInfoKHR createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            createInfo.surface = surface;
            createInfo.minImageCount = imageCount;
            createInfo.imageFormat = surfaceFormat.format;
            createInfo.imageColorSpace = surfaceFormat.colorSpace;
            createInfo.imageExtent = extent;
            createInfo.imageArrayLayers = 1;
            createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // Default to exclusive mode

            // QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
            uint32_t queueFamilyIndices[] = {
                device.getGraphicsFamilyIndex(), 
                device.getPresentFamilyIndex()
            };

            if (device.getGraphicsFamilyIndex() != device.getPresentFamilyIndex()) {
                createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
                createInfo.queueFamilyIndexCount = 2;
                createInfo.pQueueFamilyIndices = queueFamilyIndices;
            } else {
                createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
                createInfo.queueFamilyIndexCount = 0; // Optional
                createInfo.pQueueFamilyIndices = nullptr; // Optional
            }

            createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
            createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
            createInfo.presentMode = presentMode;
            createInfo.clipped = VK_TRUE;
            createInfo.oldSwapchain = VK_NULL_HANDLE;

            if (vkCreateSwapchainKHR(device.getDevice(), &createInfo, nullptr, &swapchain) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create swapchain!");
            }

            // Retrieve swap chain images
            swapChainImages = getVulkanSwapChainImages();

            // Save image format and extent
            swapChainImageFormat = surfaceFormat.format;
            swapChainExtent = extent;
            swapChainImageViews.reserve(swapChainImages.size());

            for (const auto swapChainImage : swapChainImages) {
                swapChainImageViews.emplace_back(
                    std::make_unique<VulkanImageView>(device.getDevice(), swapChainImage, swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT)
                );
            }

        }

        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
            SwapChainSupportDetails details;

            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &details.capabilities);

            uint32_t formatCount;
            vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);

            if (formatCount != 0) {
                details.formats.resize(formatCount);
                vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, details.formats.data());
            }

            uint32_t presentModeCount;
            vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);

            if (presentModeCount != 0) {
                details.presentModes.resize(presentModeCount);
                vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, details.presentModes.data());
            }

            return details;
        }

        // surface format selection
        VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
            for (const auto& availableFormat : availableFormats) {
                if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
                    availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                    return availableFormat;
                }
            }
            return availableFormats[0]; // fallback
        }

        // present mode selection
        VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes, VkPresentModeKHR preferredMode) {
            switch (preferredMode) {
                case VK_PRESENT_MODE_IMMEDIATE_KHR:
                case VK_PRESENT_MODE_MAILBOX_KHR:
                case VK_PRESENT_MODE_FIFO_KHR:
                case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
                    if (std::find(availablePresentModes.begin(), availablePresentModes.end(), preferredMode) != availablePresentModes.end()) {
                        return preferredMode;
                    }
                    break;

                default:
                    throw std::out_of_range("Unknown present mode");
            }

            // Fallback (FIFO is guaranteed to be available)
            return VK_PRESENT_MODE_FIFO_KHR;
        }

        // pick the resolution
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window) {
            if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
                return capabilities.currentExtent;
            } else {
                int width, height;
                glfwGetFramebufferSize(window, &width, &height);

                VkExtent2D actualExtent = {
                    static_cast<uint32_t>(width),
                    static_cast<uint32_t>(height)
                };

                actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
                actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

                return actualExtent;
            }
        }

        uint32_t chooseImageCount(const VkSurfaceCapabilitiesKHR& capabilities){
            uint32_t imageCount = std::max(2u, capabilities.minImageCount);

            if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
            {
                imageCount = capabilities.maxImageCount;
            }

            return imageCount;
        }

        std::vector<VkImage> getVulkanSwapChainImages()
        {
            uint32_t imageCount = 0;
            VkResult result = vkGetSwapchainImagesKHR(device.getDevice(), swapchain, &imageCount, nullptr);
            if (result != VK_SUCCESS || imageCount == 0)
            {
                throw std::runtime_error("Failed to get swapchain image count.");
            }

            std::vector<VkImage> images(imageCount);
            result = vkGetSwapchainImagesKHR(device.getDevice(), swapchain, &imageCount, images.data());

            if (result != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to get swapchain images.");
            }

            return images;
        }
};