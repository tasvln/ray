#pragma once

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <algorithm>

#include "core/window.hpp"

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class VulkanSwapChain{
    public:
        VulkanSwapChain(GLFWwindow* window, const VkDevice& device, const VkPhysicalDevice& physicalDevice, const VkSurfaceKHR& surface, const uint32_t graphicsFamilyIndex, const uint32_t presentFamilyIndex, const VkPresentModeKHR preMode): device(device) {
            createSwapChain(window, physicalDevice, surface, preMode);
        }

        ~VulkanSwapChain() {
            swapChainImages.clear();

            if (swapchain != nullptr) {
                vkDestroySwapchainKHR(device, swapchain, nullptr);
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

    private:
        VkDevice device = VK_NULL_HANDLE;
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        VkFormat swapChainImageFormat = VK_FORMAT_UNDEFINED;
        VkExtent2D swapChainExtent = {};

        std::vector<VkImage> swapChainImages;

        uint32_t graphicsFamilyIndex {};
        uint32_t presentFamilyIndex {};

        void createSwapChain(GLFWwindow* window, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkPresentModeKHR preMode) {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice, surface);

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
                graphicsFamilyIndex, 
                presentFamilyIndex
            };

            if (graphicsFamilyIndex != presentFamilyIndex) {
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

            if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create swapchain!");
            }

            // Retrieve swap chain images
            vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
            swapChainImages.resize(imageCount);

            vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapChainImages.data());

            // Save image format and extent
            swapChainImageFormat = surfaceFormat.format;
            swapChainExtent = extent;
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
};