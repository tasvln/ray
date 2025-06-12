#ifndef VULKAN_HPP
#define VULKAN_HPP

#include <volk.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <iostream>
#include <optional>
#include <string>
#include <set>
#include <vector>
#include <memory>
#include <algorithm>
#include <limits>
#include <cstdint>
#include <fstream>
#include <cstring>

#include "core/window.hpp"

// future opt note: consider using a more modern C++ Vulkan wrapper like Vulkan-Hpp
// put all infos inside their brackets: c++20
// section each parts into a separate file

const int MAX_FRAMES_IN_FLIGHT = 2;
 
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct AccelerationStructure {
    VkAccelerationStructureKHR handle;
    VkDeviceMemory memory;
    VkBuffer buffer;
    VkDeviceAddress deviceAddress;
};

// Function to create a debug messenger
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

// Function to destroy a debug messenger
void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

class VulkanContext {
    public:
        VulkanContext(const Window& window) {
            volkInitialize();
            createInstance();
            setupDebugMessenger();
            volkLoadInstance(instance);
            createSurface(window);
            pickPhysicalDevice();
            createLogicalDevice();
            volkLoadDevice(device);
            createSwapChain(window.getWindow());
            createImageViews();

            createCommandPool();

            // createRenderPass();
            // createGraphicsPipeline();
            // createFramebuffers();
            
            // ray tracing related
            createRayDescriptorSetLayout();
            createRayDescriptorPool();
            createBLAS();
            createRayOutputImage();
            createRayDescriptorSet();
            createRayPipeline();
            createShaderBindingTable();

            createCommandBuffers();
            createSyncObjects();
        }

        ~VulkanContext() {
            if (device != VK_NULL_HANDLE) {
                vkDeviceWaitIdle(device);  // Add this BEFORE destroying anything

                // --- Ray tracing cleanup ---
                if (rayOutputImageView != VK_NULL_HANDLE)
                    vkDestroyImageView(device, rayOutputImageView, nullptr);

                if (rayOutputImage != VK_NULL_HANDLE)
                    vkDestroyImage(device, rayOutputImage, nullptr);

                if (rayOutputMemory != VK_NULL_HANDLE)
                    vkFreeMemory(device, rayOutputMemory, nullptr);

                if (rayDescriptorSetLayout != VK_NULL_HANDLE)
                    vkDestroyDescriptorSetLayout(device, rayDescriptorSetLayout, nullptr);

                if (rayDescriptorPool != VK_NULL_HANDLE)
                    vkDestroyDescriptorPool(device, rayDescriptorPool, nullptr);

                if (raySBTBuffer != VK_NULL_HANDLE)
                    vkDestroyBuffer(device, raySBTBuffer, nullptr);

                if (raySBTMemory != VK_NULL_HANDLE)
                    vkFreeMemory(device, raySBTMemory, nullptr);

                if (rayPipelineLayout != VK_NULL_HANDLE)
                    vkDestroyPipelineLayout(device, rayPipelineLayout, nullptr);

                // --- BLAS cleanup ---
                if (blas.handle != VK_NULL_HANDLE) {
                    vkDestroyAccelerationStructureKHR(device, blas.handle, nullptr);
                    blas.handle = VK_NULL_HANDLE;
                }

                if (blas.buffer != VK_NULL_HANDLE) {
                    vkDestroyBuffer(device, blas.buffer, nullptr);
                    blas.buffer = VK_NULL_HANDLE;
                }

                if (blas.memory != VK_NULL_HANDLE) {
                    vkFreeMemory(device, blas.memory, nullptr);
                    blas.memory = VK_NULL_HANDLE;
                }

                blas.deviceAddress = 0; // Reset regardless; it's just a number

                // --- TLAS cleanup ---
                if (tlas.handle != VK_NULL_HANDLE) {
                    vkDestroyAccelerationStructureKHR(device, tlas.handle, nullptr);
                    tlas.handle = VK_NULL_HANDLE;
                }

                if (tlas.buffer != VK_NULL_HANDLE) {
                    vkDestroyBuffer(device, tlas.buffer, nullptr);
                    tlas.buffer = VK_NULL_HANDLE;
                }

                if (tlas.memory != VK_NULL_HANDLE) {
                    vkFreeMemory(device, tlas.memory, nullptr);
                    tlas.memory = VK_NULL_HANDLE;
                }

                tlas.deviceAddress = 0; // Reset regardless; it's just a number
                // --- End of ray tracing cleanup ---


                // Your existing cleanup continues here:
                for (size_t i = 0; i < swapChainImages.size(); i++) {
                    vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
                    vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
                }
                imageAvailableSemaphores.clear();
                renderFinishedSemaphores.clear();
                imagesInFlight.clear();

                for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                    vkDestroyFence(device, inFlightFences[i], nullptr);
                }
                inFlightFences.clear();

                if (!swapChainFramebuffers.empty()) {
                    for (auto framebuffer : swapChainFramebuffers) {
                        vkDestroyFramebuffer(device, framebuffer, nullptr);
                    }
                    swapChainFramebuffers.clear();
                }
                if (!pipelines.empty()) {
                    for (auto pipeline : pipelines) {
                        vkDestroyPipeline(device, pipeline, nullptr);
                    }
                    pipelines.clear();
                }
                if (graphicsPipelineLayout != VK_NULL_HANDLE) {
                    vkDestroyPipelineLayout(device, graphicsPipelineLayout, nullptr);
                }
                if (renderPass != VK_NULL_HANDLE) {
                    vkDestroyRenderPass(device, renderPass, nullptr);
                }
                if (!swapChainImageViews.empty()) {
                    for (auto imageView : swapChainImageViews) {
                        vkDestroyImageView(device, imageView, nullptr);
                    }
                    swapChainImageViews.clear();
                }
                if (swapChain != VK_NULL_HANDLE) {
                    vkDestroySwapchainKHR(device, swapChain, nullptr);
                }
                if (!commandBuffers.empty()) {
                    vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
                    commandBuffers.clear();
                }
                if (commandPool != VK_NULL_HANDLE) {
                    vkDestroyCommandPool(device, commandPool, nullptr);
                }
                vkDestroyDevice(device, nullptr);
            }

            if (surface != VK_NULL_HANDLE) {
                vkDestroySurfaceKHR(instance, surface, nullptr);
            }
            if (enableValidationLayers && debugMessenger != VK_NULL_HANDLE) {
                DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
            }
            if (instance != VK_NULL_HANDLE) {
                vkDestroyInstance(instance, nullptr);
            }
        }

        // main.cpp functions
        void drawFrame(GLFWwindow* window) {
            vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

            uint32_t imageIndex;
            VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

            // to handle when the window is resized
            if (result == VK_ERROR_OUT_OF_DATE_KHR) {
                recreateSwapChain(window);
                return;
            } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
                throw std::runtime_error("Failed to acquire swap chain image!");
            }

            // Wait on the image's in-flight fence if it exists
            if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
                vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
            }

            // Mark the image as now being in use by this frame
            imagesInFlight[imageIndex] = inFlightFences[currentFrame];

            vkResetFences(device, 1, &inFlightFences[currentFrame]);
        
            vkResetCommandBuffer(commandBuffers[currentFrame], 0);    
            recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

            VkSemaphore imageAvailable = imageAvailableSemaphores[currentFrame];
            VkSemaphore renderFinished = renderFinishedSemaphores[imageIndex];

            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

            VkSemaphore waitSemaphores[] = { imageAvailable };
            VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &imageAvailable;
            submitInfo.pWaitDstStageMask = waitStages;

            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

            VkSemaphore signalSemaphores[] = { renderFinished };
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = signalSemaphores;

            if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to submit draw command buffer!");
            }

            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = signalSemaphores;

            VkSwapchainKHR swapChains[] = { swapChain };
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = swapChains;
            presentInfo.pImageIndices = &imageIndex;

            presentInfo.pResults = nullptr; // Optional

            result = vkQueuePresentKHR(presentQueue, &presentInfo);

            // check if the swap chain needs to be recreated
            if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
                framebufferResized = false;
                recreateSwapChain(window);
            } else if (result != VK_SUCCESS) {
                throw std::runtime_error("Failed to present swap chain image!");
            }

            currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        }


    private:
        VkInstance instance;
        VkSurfaceKHR surface;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        VkQueue graphicsQueue = VK_NULL_HANDLE;
        VkQueue presentQueue = VK_NULL_HANDLE;

        VkDebugUtilsMessengerEXT debugMessenger;

        // Swap chain related members
        VkSwapchainKHR swapChain;
        VkFormat swapChainImageFormat;
        VkExtent2D swapChainExtent;
        std::vector<VkImage> swapChainImages;
        std::vector<VkImageView> swapChainImageViews;

        // Framebuffer related members
        VkRenderPass renderPass;
        VkFramebuffer framebuffer;
        std::vector<VkFramebuffer> swapChainFramebuffers;

        VkPipelineLayout graphicsPipelineLayout;
        std::vector<VkPipeline> pipelines;

        // command buffers related members
        VkCommandPool commandPool;
        std::vector<VkCommandBuffer> commandBuffers;

        // synchronization related members
        std::vector<VkSemaphore> imageAvailableSemaphores;
        std::vector<VkSemaphore> renderFinishedSemaphores;
        std::vector<VkFence> inFlightFences;
        std::vector<VkFence> imagesInFlight;
        size_t currentFrame = 0;

        // ray tracing related members
        VkImage rayOutputImage;
        VkDeviceMemory rayOutputMemory;
        VkImageView rayOutputImageView;
        VkDescriptorSetLayout rayDescriptorSetLayout;
        VkDescriptorPool rayDescriptorPool;
        VkDescriptorSet rayDescriptorSet;
        VkStridedDeviceAddressRegionKHR raygenRegion;
        VkStridedDeviceAddressRegionKHR missRegion;
        VkStridedDeviceAddressRegionKHR hitRegion;
        VkStridedDeviceAddressRegionKHR callableRegion{};
        VkBuffer raySBTBuffer;
        VkDeviceMemory raySBTMemory;
        VkPipelineLayout rayPipelineLayout;

        // blas related members
        // VkBuffer vertexBuffer;
        // VkDeviceMemory vertexBufferMemory;

        AccelerationStructure blas;
        AccelerationStructure tlas;


        bool framebufferResized = false;
                
        const std::vector<const char*> validationLayers = {
            "VK_LAYER_KHRONOS_validation"
        };

        const std::vector<const char*> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
            VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
            VK_KHR_SPIRV_1_4_EXTENSION_NAME,
            VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME
        };

        #ifdef NDEBUG
            const bool enableValidationLayers = false;
        #else
            const bool enableValidationLayers = true;
        #endif

        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
            QueueFamilyIndices indices;

            uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

            for (uint32_t i = 0; i < queueFamilyCount; i++) {
                if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                    indices.graphicsFamily = i;
                }

                VkBool32 presentSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
                if (presentSupport) {
                    indices.presentFamily = i;
                }

                if (indices.isComplete()) break;
            }

            return indices;
        }

        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
            SwapChainSupportDetails details;

            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

            uint32_t formatCount;
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

            if (formatCount != 0) {
                details.formats.resize(formatCount);
                vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
            }

            uint32_t presentModeCount;
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

            if (presentModeCount != 0) {
                details.presentModes.resize(presentModeCount);
                vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
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
        VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
            for (const auto& availablePresentMode : availablePresentModes) {
                if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                    return availablePresentMode;
                }
            }
            return VK_PRESENT_MODE_FIFO_KHR; // always supported (vsync)
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

        // helper function to create an image view -> used in the createImageViews() function
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

        VkShaderModule createShaderModule(const std::vector<char>& code) {
            VkShaderModuleCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            createInfo.codeSize = code.size();
            createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

            VkShaderModule shaderModule;
            if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
                throw std::runtime_error("failed to create shader module!");
            }

            return shaderModule;
        }

        VkDeviceAddress getAccelerationStructureDeviceAddress(VkAccelerationStructureKHR accelerationStructure) {
            VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
            addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
            addressInfo.accelerationStructure = accelerationStructure;

            return vkGetAccelerationStructureDeviceAddressKHR(device, &addressInfo);
        }

        static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT messageType,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData) {

            std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
            return VK_FALSE;
        }

        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
            VkPhysicalDeviceMemoryProperties memProperties;
            vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

            for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
                if ((typeFilter & (1 << i)) &&
                    (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                    return i;
                }
            }

            throw std::runtime_error("Failed to find suitable memory type!");
        }

        // read shader code from a file
        std::vector<char> readFile(const std::string& filename) {
            std::ifstream file(filename, std::ios::ate | std::ios::binary);
            if (!file.is_open()) {
                throw std::runtime_error("failed to open file!");
            }

            size_t fileSize = (size_t)file.tellg();
            std::vector<char> buffer(fileSize);

            file.seekg(0);
            file.read(buffer.data(), fileSize);

            file.close();

            return buffer;
        }

        std::vector<const char*> getRequiredExtensions() {
            uint32_t glfwExtensionCount = 0;
            const char** glfwExtensions;

            glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

            std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

            if (enableValidationLayers) {
                extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            }

            return extensions;
        }

        uint32_t alignUp(uint32_t value, uint32_t alignment) {
            return (value + alignment - 1) & ~(alignment - 1);
        }

        bool isDeviceSuitable(VkPhysicalDevice device) {
            QueueFamilyIndices indices = findQueueFamilies(device);

            bool extensionsSupported = checkDeviceExtensionSupport(device);

            bool swapChainAdequate = false;
            if (extensionsSupported) {
                SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
                swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
            }

            return indices.isComplete() && extensionsSupported && swapChainAdequate;
        }

        bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
            uint32_t extensionCount;
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

            std::vector<VkExtensionProperties> availableExtensions(extensionCount);
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

            std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

            for (const auto& extension : availableExtensions) {
                requiredExtensions.erase(extension.extensionName);
            }

            return requiredExtensions.empty();
        }

        bool checkValidationLayerSupport() {
            uint32_t layerCount;
            vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

            std::vector<VkLayerProperties> availableLayers(layerCount);
            vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

            for (const char* layerName : validationLayers) {
                bool layerFound = false;

                for (const auto& layerProperties : availableLayers) {
                    if (strcmp(layerName, layerProperties.layerName) == 0) {
                        layerFound = true;
                        break;
                    }
                }

                if (!layerFound) {
                    return false;
                }
            }

            return true;
        }

        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
            createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            createInfo.messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            createInfo.messageType =
                VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            createInfo.pfnUserCallback = debugCallback;  // Your debug callback function
            createInfo.pUserData = nullptr; // Optional
        }

        
        void validateSBTRegions(uint32_t groupCount, uint32_t handleAlignment) {
            // assert(groupCount >= 3 && "Expected at least 3 shader groups: raygen, miss, hit");

            std::cout << "Validating SBT regions...\n";

            if (groupCount < 3) {
                throw std::runtime_error("Expected at least 3 shader groups: raygen, miss, hit");
            }

            if (raygenRegion.size != handleAlignment ||
                missRegion.size != handleAlignment ||
                hitRegion.size != handleAlignment) {
                throw std::runtime_error("SBT region sizes do not match handle alignment!");
            }

            if (raygenRegion.deviceAddress != missRegion.deviceAddress - handleAlignment ||
                raygenRegion.deviceAddress != hitRegion.deviceAddress - 2 * handleAlignment) {
                // Or more generally, check that the offsets are aligned and sequential:
                VkDeviceAddress base = raygenRegion.deviceAddress;
                if (missRegion.deviceAddress != base + handleAlignment ||
                    hitRegion.deviceAddress != base + 2 * handleAlignment) {
                    throw std::runtime_error("SBT region device addresses are not sequential!");
                }
            }

            std::cout << "SBT regions validated: handleAlignment = " << handleAlignment << "\n";
        }

        // Create a Vulkan instance with the required extensions and validation layers
        void createInstance() {
            if (enableValidationLayers && !checkValidationLayerSupport()) {
                throw std::runtime_error("Validation layers requested, but not available!");
            }

            VkApplicationInfo appInfo{};
            appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            appInfo.pApplicationName = "Ray";
            appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
            appInfo.pEngineName = "No Engine";
            appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
            appInfo.apiVersion = VK_API_VERSION_1_3;

            VkInstanceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            createInfo.pApplicationInfo = &appInfo;

            auto extensions = getRequiredExtensions();
            createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
            createInfo.ppEnabledExtensionNames = extensions.data();

            VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
            if (enableValidationLayers) {
                createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
                createInfo.ppEnabledLayerNames = validationLayers.data();

                populateDebugMessengerCreateInfo(debugCreateInfo);  // You must implement this
                createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
            } else {
                createInfo.enabledLayerCount = 0;
                createInfo.pNext = nullptr;
            }

            if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create Vulkan instance");
            }
        }

        void setupDebugMessenger() {
            if (!enableValidationLayers) return;

            VkDebugUtilsMessengerCreateInfoEXT createInfo;
            populateDebugMessengerCreateInfo(createInfo);

            VkResult result = CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger);
            if (result != VK_SUCCESS) {
                std::cerr << "Failed to create debug messenger! Error code: " << result << std::endl;
                // Optional: still throw if you want to halt on failure
                throw std::runtime_error("Failed to set up debug messenger");
            }

            std::cout << "Debug messenger created successfully." << std::endl;
        }

        void createSurface(const Window& window) {
            if (glfwCreateWindowSurface(instance, window.getWindow(), nullptr, &surface) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create window surface");
            }
        }

        // Pick a physical device that supports the required features
        void pickPhysicalDevice() {
            uint32_t deviceCount = 0;
            vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

            if (deviceCount == 0) {
                throw std::runtime_error("Failed to find GPUs with Vulkan support");
            }

            std::vector<VkPhysicalDevice> devices(deviceCount);
            vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

            
            for (const auto& dev : devices) {
                if (isDeviceSuitable(dev)) {
                    physicalDevice = dev;
                    break;
                }
            }


            if (physicalDevice == VK_NULL_HANDLE) {
                throw std::runtime_error("Failed to find a suitable GPU");
            }
        }

        // Create a logical device with graphics and present queues
        void createLogicalDevice() {
            QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

            std::set<uint32_t> uniqueFamilies = {
                indices.graphicsFamily.value(),
                indices.presentFamily.value()
            };


            float queuePriority = 1.0f;
            std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

            for (uint32_t family : uniqueFamilies) {
                VkDeviceQueueCreateInfo queueCreateInfo{};
                queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queueCreateInfo.queueFamilyIndex = family;
                queueCreateInfo.queueCount = 1;
                queueCreateInfo.pQueuePriorities = &queuePriority;
                queueCreateInfos.push_back(queueCreateInfo);
            }

            // 1. Setup feature structs
            VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{};
            bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
            bufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;

            VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures{};
            rayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
            rayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
            rayTracingPipelineFeatures.pNext = &bufferDeviceAddressFeatures;

            VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{};
            accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
            accelerationStructureFeatures.accelerationStructure = VK_TRUE;
            accelerationStructureFeatures.pNext = &rayTracingPipelineFeatures;

            // 2. Chain features into VkPhysicalDeviceFeatures2
            VkPhysicalDeviceFeatures2 deviceFeatures2{};
            deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            deviceFeatures2.pNext = &accelerationStructureFeatures;


            VkDeviceCreateInfo deviceInfo{};
            deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            deviceInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
            deviceInfo.pQueueCreateInfos = queueCreateInfos.data();

            deviceInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
            deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();

            deviceInfo.pEnabledFeatures = nullptr;
            deviceInfo.pNext = &deviceFeatures2;

            if (vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create logical device");
            }

            vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
            vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);

        }

        // create a swap chain for rendering
        void createSwapChain(GLFWwindow* window) {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

            VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
            VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
            VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities, window);

            uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
            if (swapChainSupport.capabilities.maxImageCount > 0 &&
                imageCount > swapChainSupport.capabilities.maxImageCount) {
                imageCount = swapChainSupport.capabilities.maxImageCount;
            }

            VkSwapchainCreateInfoKHR createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            createInfo.surface = surface;

            createInfo.minImageCount = imageCount;
            createInfo.imageFormat = surfaceFormat.format;
            createInfo.imageColorSpace = surfaceFormat.colorSpace;
            createInfo.imageExtent = extent;
            createInfo.imageArrayLayers = 1;
            createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

            QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
            uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

            if (indices.graphicsFamily != indices.presentFamily) {
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

            if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create swapchain!");
            }

            // Retrieve swap chain images
            vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
            swapChainImages.resize(imageCount);
            vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

            // Save image format and extent
            swapChainImageFormat = surfaceFormat.format;
            swapChainExtent = extent;
        }

        void createImageViews() {
            swapChainImageViews.resize(swapChainImages.size());

            for (size_t i = 0; i < swapChainImages.size(); i++) {
                swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat);
            }
        }

        void createRenderPass() {
            // Color attachment
            VkAttachmentDescription colorAttachment{};
            colorAttachment.format = swapChainImageFormat;
            colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

            colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

            colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

            // Attachment reference
            VkAttachmentReference colorAttachmentRef{};
            colorAttachmentRef.attachment = 0;
            colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            // Subpass
            VkSubpassDescription subpass{};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &colorAttachmentRef;

            // Subpass dependency (synchronization)
            VkSubpassDependency dependency{};
            dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            dependency.dstSubpass = 0;

            dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.srcAccessMask = 0;

            dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            // Create Render Pass
            VkRenderPassCreateInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            renderPassInfo.attachmentCount = 1;
            renderPassInfo.pAttachments = &colorAttachment;
            renderPassInfo.subpassCount = 1;
            renderPassInfo.pSubpasses = &subpass;
            renderPassInfo.dependencyCount = 1;
            renderPassInfo.pDependencies = &dependency;

            if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create render pass");
            }
        }

        void createFramebuffers() {
            swapChainFramebuffers.resize(swapChainImageViews.size());

            for (size_t i = 0; i < swapChainImageViews.size(); i++) {
                VkImageView attachments[] = {
                    swapChainImageViews[i]
                };

                VkFramebufferCreateInfo framebufferInfo{};
                framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                framebufferInfo.renderPass = renderPass;
                framebufferInfo.attachmentCount = 1;
                framebufferInfo.pAttachments = attachments;
                framebufferInfo.width = swapChainExtent.width;
                framebufferInfo.height = swapChainExtent.height;
                framebufferInfo.layers = 1;

                if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
                    throw std::runtime_error("Failed to create framebuffer!");
                }
            }
        }

        // creating this to create the raytracing pipeline
        void createRayPipeline() {
            auto rgenShader = readFile("shaders/ray/rgen.spv");
            auto rmissShader = readFile("shaders/ray/rmiss.spv");
            auto rchitShader = readFile("shaders/ray/rchit.spv");

            VkShaderModule rgenModule = createShaderModule(rgenShader);
            VkShaderModule rmissModule = createShaderModule(rmissShader);
            VkShaderModule rchitModule = createShaderModule(rchitShader);

            VkPipelineShaderStageCreateInfo rgenStageInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                .module = rgenModule,
                .pName = "main",
            };

            VkPipelineShaderStageCreateInfo rmissStageInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_MISS_BIT_KHR,
                .module = rmissModule,
                .pName = "main",
            };

            VkPipelineShaderStageCreateInfo rchitStageInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                .module = rchitModule,
                .pName = "main",
            };

            VkPipelineShaderStageCreateInfo shaderStages[] = {rgenStageInfo, rmissStageInfo, rchitStageInfo};
            
            VkRayTracingShaderGroupCreateInfoKHR groups[3] = {};

            // Raygen group
            groups[0].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            groups[0].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
            groups[0].generalShader = 0; // index in shaderStages[]
            groups[0].closestHitShader = VK_SHADER_UNUSED_KHR;
            groups[0].anyHitShader = VK_SHADER_UNUSED_KHR;
            groups[0].intersectionShader = VK_SHADER_UNUSED_KHR;

            // Miss group
            groups[1].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            groups[1].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
            groups[1].generalShader = 1; // index in shaderStages[]
            groups[1].closestHitShader = VK_SHADER_UNUSED_KHR;
            groups[1].anyHitShader = VK_SHADER_UNUSED_KHR;
            groups[1].intersectionShader = VK_SHADER_UNUSED_KHR;

            // Closest hit group
            groups[2].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            groups[2].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
            groups[2].generalShader = VK_SHADER_UNUSED_KHR;
            groups[2].closestHitShader = 2; // index in shaderStages[]
            groups[2].anyHitShader = VK_SHADER_UNUSED_KHR;
            groups[2].intersectionShader = VK_SHADER_UNUSED_KHR;

            VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = 1;
            pipelineLayoutInfo.pSetLayouts = &rayDescriptorSetLayout;
            // Add push constants if needed here.

            if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &rayPipelineLayout) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create pipeline layout!");
            }

            // Ray tracing pipeline creation
            VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
            pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
            pipelineInfo.stageCount = 3;
            pipelineInfo.pStages = shaderStages;
            pipelineInfo.groupCount = 3;
            pipelineInfo.pGroups = groups;
            pipelineInfo.maxPipelineRayRecursionDepth = 1; // Maximum recursion depth for ray tracing
            pipelineInfo.layout = rayPipelineLayout;
            pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // No base pipeline
            pipelineInfo.basePipelineIndex = -1; // No base pipeline index
            pipelines.resize(1); // Resize to hold the ray tracing pipeline

            if (vkCreateRayTracingPipelinesKHR(device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipelines[0]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create ray tracing pipeline!");
            }

            // Cleanup shader modules
            vkDestroyShaderModule(device, rgenModule, nullptr);
            vkDestroyShaderModule(device, rmissModule, nullptr);
            vkDestroyShaderModule(device, rchitModule, nullptr);
        }

        void createRayOutputImage() {
            // Create an image for ray tracing output
            VkImageCreateInfo imageInfo{};
            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM; // Use a format suitable for ray tracing output
            imageInfo.extent.width = swapChainExtent.width;
            imageInfo.extent.height = swapChainExtent.height;
            imageInfo.extent.depth = 1;
            imageInfo.mipLevels = 1;
            imageInfo.arrayLayers = 1;
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT; // Storage for ray tracing output
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            if (vkCreateImage(device, &imageInfo, nullptr, &rayOutputImage) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create ray output image!");
            }

            // Allocate memory for the ray output image
            VkMemoryRequirements memRequirements;
            vkGetImageMemoryRequirements(device, rayOutputImage, &memRequirements);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memRequirements.size;

            // Find suitable memory type
            VkPhysicalDeviceMemoryProperties memProperties;
            vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
            
            allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            if (vkAllocateMemory(device, &allocInfo, nullptr, &rayOutputMemory) != VK_SUCCESS) {
                throw std::runtime_error("Failed to allocate ray output memory!");
            }

            vkBindImageMemory(device, rayOutputImage, rayOutputMemory, 0);

            // Create an image view for the ray output image
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = rayOutputImage;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM; // Same format as the image
            viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(device, &viewInfo, nullptr, &rayOutputImageView) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create ray output image view!");
            }
        }

        void createRayDescriptorSetLayout() {
            // Create a descriptor set layout for ray tracing
            VkDescriptorSetLayoutBinding binding{};
            binding.binding = 0;
            binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; // Storage image for ray tracing output
            binding.descriptorCount = 1;
            binding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR; // Used in ray generation shader
            binding.pImmutableSamplers = nullptr; // Optional

            VkDescriptorSetLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutInfo.bindingCount = 1;
            layoutInfo.pBindings = &binding;

            if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &rayDescriptorSetLayout) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create ray descriptor set layout!");
            }
        }

        void createRayDescriptorPool() {
            // Create a descriptor pool for ray tracing
            VkDescriptorPoolSize poolSize{};
            poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; // Storage image for ray tracing output
            poolSize.descriptorCount = 1; // Only one storage image

            VkDescriptorPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            poolInfo.poolSizeCount = 1;
            poolInfo.pPoolSizes = &poolSize;
            poolInfo.maxSets = 1; // Only one descriptor set

            if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &rayDescriptorPool) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create ray descriptor pool!");
            }
        }

        void createRayDescriptorSet() {
            // Allocate a descriptor set from the ray descriptor pool
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = rayDescriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &rayDescriptorSetLayout;

            if (vkAllocateDescriptorSets(device, &allocInfo, &rayDescriptorSet) != VK_SUCCESS) {
                throw std::runtime_error("Failed to allocate ray descriptor set!");
            }

            VkDescriptorImageInfo storageImageInfo{};
            storageImageInfo.imageView = rayOutputImageView;
            storageImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = rayDescriptorSet;
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pImageInfo = &storageImageInfo;

            vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
        }

        void createShaderBindingTable() {
            // 1. Query SBT handle size and alignment
            VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProperties{};
            rayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

            VkPhysicalDeviceProperties2 deviceProperties2{};
            deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            deviceProperties2.pNext = &rayTracingProperties;
            vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProperties2);

            uint32_t handleSize = rayTracingProperties.shaderGroupHandleSize;
            uint32_t handleAlignment = rayTracingProperties.shaderGroupBaseAlignment;

            // 2. Calculate sizes with proper alignment
            uint32_t groupCount = 3;
            uint32_t handleSizeAligned = alignUp(handleSize, handleAlignment);
            // uint32_t sbtSize = groupCount * handleSizeAligned;
            uint32_t sbtSize = groupCount * handleAlignment;

            // 3. Get shader group handles from the pipeline
            std::vector<uint8_t> shaderHandleStorage(sbtSize);

            if (shaderHandleStorage.size() < groupCount * handleAlignment) {
                throw std::runtime_error("Shader handle storage size too small!");
            }

            if (vkGetRayTracingShaderGroupHandlesKHR(device, pipelines[0], 0, groupCount, sbtSize, shaderHandleStorage.data()) != VK_SUCCESS) {
                throw std::runtime_error("Failed to get shader group handles!");
            }

            // 4. Create SBT buffer
            createBuffer(
                sbtSize,
                VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                raySBTBuffer,
                raySBTMemory,
                true // Use device address for SBT
            );

            // 5. Copy shader group handles into buffer
            void* mappedData;
            vkMapMemory(device, raySBTMemory, 0, sbtSize, 0, &mappedData);
            memcpy(mappedData, shaderHandleStorage.data(), sbtSize);
            vkUnmapMemory(device, raySBTMemory);

            // 6. Get device address of SBT buffer
            VkBufferDeviceAddressInfo addressInfo{};
            addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            addressInfo.buffer = raySBTBuffer;
            VkDeviceAddress sbtAddress = vkGetBufferDeviceAddress(device, &addressInfo);

            // 7. Fill in strided regions (used in vkCmdTraceRaysKHR)
            raygenRegion.deviceAddress = sbtAddress + 0 * handleAlignment;
            raygenRegion.stride = handleAlignment;
            raygenRegion.size = handleAlignment;

            missRegion.deviceAddress = sbtAddress + 1 * handleAlignment;
            missRegion.stride = handleAlignment;
            missRegion.size = handleAlignment;

            hitRegion.deviceAddress = sbtAddress + 2 * handleAlignment;
            hitRegion.stride = handleAlignment;
            hitRegion.size = handleAlignment;

            // Optional: callable shaders
            callableRegion.deviceAddress = 0;
            callableRegion.stride = 0;
            callableRegion.size = 0;

            // Validate SBT regions
            validateSBTRegions(groupCount, handleAlignment);
        }

        void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory, bool useDeviceAddress = false) {
            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = size;
            bufferInfo.usage = usage;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create buffer!");
            }

            VkMemoryRequirements memRequirements;
            vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memRequirements.size;
            allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

            VkMemoryAllocateFlagsInfo allocFlagsInfo{};
            if (useDeviceAddress) {
                allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
                allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
                allocFlagsInfo.pNext = nullptr;
                allocInfo.pNext = &allocFlagsInfo;
            }

            if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
                throw std::runtime_error("Failed to allocate buffer memory!");
            }

            if (vkBindBufferMemory(device, buffer, bufferMemory, 0) != VK_SUCCESS) {
                throw std::runtime_error("Failed to bind buffer memory!");
            }
        }

        void createBLAS() {
            // Define geometry (e.g., 1 triangle)
            std::vector<float> vertices = {
                -1.0f, -1.0f, 0.0f,
                1.0f, -1.0f, 0.0f,
                0.0f,  1.0f, 0.0f
            };


            VkBuffer vertexBuffer;
            VkDeviceMemory vertexMemory;
            VkDeviceSize bufferSize = sizeof(float) * vertices.size();

            createBuffer(bufferSize,
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                vertexBuffer,
                vertexMemory,
                true);

            // Copy vertex data
            void* data;
            vkMapMemory(device, vertexMemory, 0, bufferSize, 0, &data);
            memcpy(data, vertices.data(), (size_t)bufferSize);
            vkUnmapMemory(device, vertexMemory);

            // Get buffer device address
            VkBufferDeviceAddressInfo addressInfo{};
            addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            addressInfo.buffer = vertexBuffer;
            VkDeviceAddress vertexAddress = vkGetBufferDeviceAddress(device, &addressInfo);

            // Geometry info
            VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
            triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
            triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
            triangles.vertexData.deviceAddress = vertexAddress;
            triangles.vertexStride = sizeof(float) * 3;
            triangles.maxVertex = 3;

            triangles.indexType = VK_INDEX_TYPE_NONE_KHR;
            triangles.indexData.deviceAddress = 0;

            VkAccelerationStructureGeometryKHR geometry{};
            geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
            geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
            geometry.geometry.triangles = triangles;
            geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

            // Build info
            VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
            buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
            buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
            buildInfo.geometryCount = 1;
            buildInfo.pGeometries = &geometry;

            uint32_t primitiveCount = 1;
            VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
            sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

            vkGetAccelerationStructureBuildSizesKHR(
                device,
                VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                &buildInfo,
                &primitiveCount,
                &sizeInfo);

            // Create acceleration structure
            blas = createAccelerationStructure(sizeInfo.accelerationStructureSize, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR);

            // Scratch buffer
            VkBuffer scratchBuffer;
            VkDeviceMemory scratchMemory;
            createBuffer(sizeInfo.buildScratchSize,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                        scratchBuffer,
                        scratchMemory,
                        true);

            VkBufferDeviceAddressInfo scratchAddressInfo{};
            scratchAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            scratchAddressInfo.buffer = scratchBuffer;
            VkDeviceAddress scratchAddress = vkGetBufferDeviceAddress(device, &scratchAddressInfo);

            // Build command
            buildInfo.dstAccelerationStructure = blas.handle;
            buildInfo.scratchData.deviceAddress = scratchAddress;

            VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
            rangeInfo.primitiveCount = primitiveCount;
            const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;

            VkCommandBuffer cmd = beginSingleTimeCommands();
            vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRangeInfo);
            endSingleTimeCommands(cmd);

            // Clean up scratch
            vkDestroyBuffer(device, scratchBuffer, nullptr);
            vkFreeMemory(device, scratchMemory, nullptr);
        }

        void createTLAS() {
            // Instance info
            VkAccelerationStructureInstanceKHR instance{};

            VkTransformMatrixKHR transformMatrix = {
                {
                    {1.0f, 0.0f, 0.0f, 0.0f},
                    {0.0f, 1.0f, 0.0f, 0.0f},
                    {0.0f, 0.0f, 1.0f, 0.0f}
                }
            };
            instance.transform = transformMatrix;

            instance.instanceCustomIndex = 0;
            instance.mask = 0xFF;
            instance.instanceShaderBindingTableRecordOffset = 0;
            instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            instance.accelerationStructureReference = getAccelerationStructureDeviceAddress(blas.handle);

            // Upload instance to buffer
            VkBuffer instanceBuffer;
            VkDeviceMemory instanceMemory;
            createBuffer(sizeof(instance),
                        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        instanceBuffer,
                        instanceMemory,
                        true);

            void* data;
            vkMapMemory(device, instanceMemory, 0, sizeof(instance), 0, &data);
            memcpy(data, &instance, sizeof(instance));
            vkUnmapMemory(device, instanceMemory);

            VkBufferDeviceAddressInfo addressInfo{};
            addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            addressInfo.buffer = instanceBuffer;
            VkDeviceAddress instanceAddress = vkGetBufferDeviceAddress(device, &addressInfo);

            // TLAS geometry
            VkAccelerationStructureGeometryKHR geometry{};
            geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
            geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
            geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
            geometry.geometry.instances.arrayOfPointers = VK_FALSE;
            geometry.geometry.instances.data.deviceAddress = instanceAddress;

            VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
            buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
            buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
            buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
            buildInfo.geometryCount = 1;
            buildInfo.pGeometries = &geometry;

            uint32_t primitiveCount = 1;
            VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
            sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

            vkGetAccelerationStructureBuildSizesKHR(
                device,
                VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                &buildInfo,
                &primitiveCount,
                &sizeInfo);

            // Create TLAS
            tlas = createAccelerationStructure(sizeInfo.accelerationStructureSize, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR);

            // Scratch buffer
            VkBuffer scratchBuffer;
            VkDeviceMemory scratchMemory;
            createBuffer(sizeInfo.buildScratchSize,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                        scratchBuffer,
                        scratchMemory,
                        true);

            VkBufferDeviceAddressInfo scratchAddressInfo{};
            scratchAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            scratchAddressInfo.buffer = scratchBuffer;
            VkDeviceAddress scratchAddress = vkGetBufferDeviceAddress(device, &scratchAddressInfo);

            buildInfo.dstAccelerationStructure = tlas.handle;
            buildInfo.scratchData.deviceAddress = scratchAddress;

            VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
            rangeInfo.primitiveCount = primitiveCount;
            const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;

            VkCommandBuffer cmd = beginSingleTimeCommands();
            vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRangeInfo);
            endSingleTimeCommands(cmd);

            // Clean up scratch
            vkDestroyBuffer(device, scratchBuffer, nullptr);
            vkFreeMemory(device, scratchMemory, nullptr);
        }

        VkCommandBuffer beginSingleTimeCommands() {
            VkCommandBufferAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandPool = commandPool; // must be created with VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
            allocInfo.commandBufferCount = 1;

            VkCommandBuffer commandBuffer;
            vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

            vkBeginCommandBuffer(commandBuffer, &beginInfo);

            return commandBuffer;
        }

        void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
            vkEndCommandBuffer(commandBuffer);

            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;

            vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
            vkQueueWaitIdle(graphicsQueue);

            vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        }

        AccelerationStructure createAccelerationStructure(
            VkDeviceSize size,
            VkAccelerationStructureTypeKHR type)
        {
            AccelerationStructure accelStruct{};

            // 1. Create backing buffer
            createBuffer(
                size,
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                accelStruct.buffer,
                accelStruct.memory,
                true
            );

            // 2. Create the acceleration structure
            VkAccelerationStructureCreateInfoKHR createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
            createInfo.buffer = accelStruct.buffer;
            createInfo.size = size;
            createInfo.type = type;

            if (vkCreateAccelerationStructureKHR(device, &createInfo, nullptr, &accelStruct.handle) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create acceleration structure!");
            }

            // 3. Get device address
            VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
            addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
            addressInfo.accelerationStructure = accelStruct.handle;

            accelStruct.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &addressInfo);

            return accelStruct;
        }

        // so graphics pipeline is used to render vertices and fragments
        void createGraphicsPipeline() {
            auto vertShaderCode = readFile("shaders/vert.spv");
            auto fragShaderCode = readFile("shaders/frag.spv");

            VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
            VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

            VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
            vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
            vertShaderStageInfo.module = vertShaderModule;
            vertShaderStageInfo.pName = "main";

            VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
            fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            fragShaderStageInfo.module = fragShaderModule;
            fragShaderStageInfo.pName = "main";

            VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

            // dynamic state
            std::vector<VkDynamicState> dynamicStates = {
                VK_DYNAMIC_STATE_VIEWPORT,
                VK_DYNAMIC_STATE_SCISSOR
            };

            VkPipelineDynamicStateCreateInfo dynamicState{};
            dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
            dynamicState.pDynamicStates = dynamicStates.data();

            // vertex input state
            VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
            vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertexInputInfo.vertexBindingDescriptionCount = 0;
            vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
            vertexInputInfo.vertexAttributeDescriptionCount = 0;
            vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional 

            // input assembly state
            VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
            inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            inputAssembly.primitiveRestartEnable = VK_FALSE;

            // viewport state
            // VkViewport viewport{}; would be used in the command buffer
            // viewport.x = 0.0f;
            // viewport.y = 0.0f;
            // viewport.width = (float) swapChainExtent.width;
            // viewport.height = (float) swapChainExtent.height;
            // viewport.minDepth = 0.0f;
            // viewport.maxDepth = 1.0f;

            // VkRect2D scissor{};
            // scissor.offset = {0, 0};
            // scissor.extent = swapChainExtent;

            VkPipelineViewportStateCreateInfo viewportState{};
            viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewportState.viewportCount = 1;
            viewportState.scissorCount = 1;

            // rasterization state
            VkPipelineRasterizationStateCreateInfo rasterizer{};
            rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rasterizer.depthClampEnable = VK_FALSE;
            rasterizer.rasterizerDiscardEnable = VK_FALSE;
            rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
            rasterizer.lineWidth = 1.0f;
            rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
            rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
            rasterizer.depthBiasEnable = VK_FALSE;
            rasterizer.depthBiasConstantFactor = 0.0f; // Optional
            rasterizer.depthBiasClamp = 0.0f; // Optional
            rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

            // multisampling state
            VkPipelineMultisampleStateCreateInfo multisampling{};
            multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multisampling.sampleShadingEnable = VK_FALSE;
            multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            multisampling.minSampleShading = 1.0f; // Optional
            multisampling.pSampleMask = nullptr; // Optional
            multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
            multisampling.alphaToOneEnable = VK_FALSE; // Optional

            // color blending state
            VkPipelineColorBlendAttachmentState colorBlendAttachment{};
            colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            colorBlendAttachment.blendEnable = VK_FALSE;
            colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
            colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
            colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
            colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
            colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
            colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

            VkPipelineColorBlendStateCreateInfo colorBlending{};
            colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlending.logicOpEnable = VK_FALSE;
            colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
            colorBlending.attachmentCount = 1;
            colorBlending.pAttachments = &colorBlendAttachment;
            colorBlending.blendConstants[0] = 0.0f; // Optional
            colorBlending.blendConstants[1] = 0.0f; // Optional
            colorBlending.blendConstants[2] = 0.0f; // Optional
            colorBlending.blendConstants[3] = 0.0f; // Optional

            // pipeline layout
            VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = 0; // Optional
            pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
            pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
            pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

            if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &graphicsPipelineLayout) != VK_SUCCESS) {
                throw std::runtime_error("failed to create pipeline layout!");
            }

            // Graphics pipeline creation
            VkGraphicsPipelineCreateInfo pipelineInfo{};
            pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipelineInfo.stageCount = 2;
            pipelineInfo.pStages = shaderStages;
            pipelineInfo.pVertexInputState = &vertexInputInfo;
            pipelineInfo.pInputAssemblyState = &inputAssembly;
            pipelineInfo.pViewportState = &viewportState;
            pipelineInfo.pRasterizationState = &rasterizer;
            pipelineInfo.pMultisampleState = &multisampling;
            pipelineInfo.pDepthStencilState = nullptr; // Optional
            pipelineInfo.pColorBlendState = &colorBlending;
            pipelineInfo.pDynamicState = &dynamicState;
            pipelineInfo.layout = graphicsPipelineLayout;
            pipelineInfo.renderPass = renderPass;
            pipelineInfo.subpass = 0;
            pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
            pipelineInfo.basePipelineIndex = -1; // Optional

            pipelines.resize(1); // Resize to hold one pipeline

            if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipelines[0]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create graphics pipeline");
            }

            // Cleanup shader modules
            vkDestroyShaderModule(device, fragShaderModule, nullptr);
            vkDestroyShaderModule(device, vertShaderModule, nullptr);
        }

        void createCommandPool() {
            QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

            VkCommandPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // Optional
            poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
            
            if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create command pool");
            }
        }

        void createCommandBuffers() {
            commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

            VkCommandBufferAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.commandPool = commandPool;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

            if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
                throw std::runtime_error("Failed to allocate command buffers");
            }
        }

        void createSyncObjects() {
            imageAvailableSemaphores.resize(swapChainImages.size());
            renderFinishedSemaphores.resize(swapChainImages.size());
            imagesInFlight.resize(swapChainImages.size(), VK_NULL_HANDLE);

            inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

            VkSemaphoreCreateInfo semaphoreInfo{};
            semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

            VkFenceCreateInfo fenceInfo{};
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // So first frame doesn't hang

            for (size_t i = 0; i < swapChainImages.size(); i++) {
                if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                    vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS) {
                    throw std::runtime_error("Failed to create semaphores for a swap chain image!");
                }
            }

            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                if (vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
                    throw std::runtime_error("Failed to create in-flight fence!");
                }
            }
        }

        // main.cpp functions
        void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = 0; // Optional
            beginInfo.pInheritanceInfo = nullptr; // Optional

            if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
                throw std::runtime_error("failed to begin recording command buffer!");
            }

            transitionImageLayout(commandBuffer, rayOutputImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

            // VkRenderPassBeginInfo renderPassInfo{};
            // renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            // renderPassInfo.renderPass = renderPass;
            // renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
            // renderPassInfo.renderArea.offset = {0, 0};
            // renderPassInfo.renderArea.extent = swapChainExtent;

            // VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
            // renderPassInfo.clearValueCount = 1;
            // renderPassInfo.pClearValues = &clearColor;

            // vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            // vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[0]);
            
            // VkViewport viewport{};
            // viewport.x = 0.0f;
            // viewport.y = 0.0f;
            // viewport.width  = (float) swapChainExtent.width;
            // viewport.height = (float) swapChainExtent.height;
            // viewport.minDepth = 0.0f;
            // viewport.maxDepth = 1.0f;
            // vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

            // VkRect2D scissor{};
            // scissor.offset = {0, 0};
            // scissor.extent = swapChainExtent;
            // vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            // vkCmdDraw(commandBuffer, 3, 1, 0, 0);
            // vkCmdEndRenderPass(commandBuffer);

            // bind ray tracing pipeline
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelines[0]);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rayPipelineLayout, 0, 1, &rayDescriptorSet, 0, nullptr);
            
            // if (vkCmdTraceRaysKHR(commandBuffer, &raygenRegion, &missRegion, &hitRegion, &callableRegion, swapChainExtent.width, swapChainExtent.height, 1) != VK_SUCCESS) {
            //     throw std::runtime_error("Failed to trace rays!");
            // }
            vkCmdTraceRaysKHR(commandBuffer, &raygenRegion, &missRegion, &hitRegion, &callableRegion, swapChainExtent.width, swapChainExtent.height, 1);
            
            transitionImageLayout(commandBuffer, swapChainImages[imageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

            if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
                throw std::runtime_error("failed to record command buffer!");
            }
        }

        void transitionImageLayout(
            VkCommandBuffer commandBuffer,
            VkImage image,
            VkImageLayout oldLayout,
            VkImageLayout newLayout)
        {
            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = oldLayout;
            barrier.newLayout = newLayout;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;

            VkPipelineStageFlags srcStage;
            VkPipelineStageFlags dstStage;

            if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

                srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                dstStage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
            } else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = 0;

                srcStage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
                dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            } else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
                barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                barrier.dstAccessMask = 0;

                srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            } else {
                throw std::invalid_argument("unsupported layout transition!");
            }

            vkCmdPipelineBarrier(
                commandBuffer,
                srcStage,
                dstStage,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );
        }

        // function to handle window resizing
        void recreateSwapChain(GLFWwindow* window) {
            // Make sure window is not minimized
            int width = 0, height = 0;
            glfwGetFramebufferSize(window, &width, &height);
            while (width == 0 || height == 0) {
                glfwGetFramebufferSize(window, &width, &height);
                glfwWaitEvents();
            }

            vkDeviceWaitIdle(device);

            if (!commandBuffers.empty()) {
                vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
                commandBuffers.clear();
            }

            cleanupSwapChain();     // You'll need to write this
            createSwapChain(window); // Already done
            createImageViews();     // Already done
            createRenderPass();
            createGraphicsPipeline(); // Already done
            createFramebuffers();   // Already done
            
            createCommandBuffers(); // Already done
        }

        void cleanupSwapChain() {
            for (auto framebuffer : swapChainFramebuffers) {
                vkDestroyFramebuffer(device, framebuffer, nullptr);
            }
            swapChainFramebuffers.clear();

            for (auto imageView : swapChainImageViews) {
                vkDestroyImageView(device, imageView, nullptr);
            }
            swapChainImageViews.clear();

            if (!pipelines.empty()) {
                for (auto pipeline : pipelines) {
                    vkDestroyPipeline(device, pipeline, nullptr);
                }
                pipelines.clear();
            }

            if (rayPipelineLayout != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(device, rayPipelineLayout, nullptr);
            }

            if (graphicsPipelineLayout != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(device, graphicsPipelineLayout, nullptr);
            }

            if (renderPass != VK_NULL_HANDLE) {
                vkDestroyRenderPass(device, renderPass, nullptr);
                renderPass = VK_NULL_HANDLE;
            }

            if (swapChain != VK_NULL_HANDLE) {
                vkDestroySwapchainKHR(device, swapChain, nullptr);
                swapChain = VK_NULL_HANDLE;
            }
        }


};

#endif