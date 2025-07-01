#ifndef VULKAN_HPP
#define VULKAN_HPP

#include <volk.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

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
#include <array>

#include "core/window.hpp"

// future opt note: consider using a more modern C++ Vulkan wrapper like Vulkan-Hpp
// put all infos inside their brackets: c++20
// section each parts into a separate file

#define MATERIAL_LAMBERT 0
#define MATERIAL_MIRROR  1
#define MATERIAL_METAL   2
#define MATERIAL_DIELECTRIC 3
#define MATERIAL_EMISSIVE   4
 
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value(); // extend if needed
    }
};


struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct AccelerationStructure {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
    VkDeviceAddress deviceAddress = 0;
};

struct FrameSync {
    VkSemaphore imageAvailable = VK_NULL_HANDLE;     // from vkAcquireNextImageKHR
    VkSemaphore renderFinished = VK_NULL_HANDLE;     // signal to present
    VkFence inFlightFence = VK_NULL_HANDLE;
};

struct Material {
    glm::vec4 albedo;
    glm::vec4 emission;
    glm::vec4 rmix;
    uint32_t type;
};

struct LayoutTransition {
    VkAccessFlags srcAccess;
    VkAccessFlags dstAccess;
    VkPipelineStageFlags srcStage;
    VkPipelineStageFlags dstStage;
};

enum PipelineType {
    PIPELINE_RAY_TRACING = 0,
    PIPELINE_FULLSCREEN = 1
};

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

            createRenderPass();

            createSampler();
            
            // ray tracing related
            createRayDescriptorSetLayout();
            createFullscreenDescriptorSetLayout();
            
            pipelines.resize(2);

            createRayDescriptorPool();
            createFullscreenDescriptorPool();

            createCommandPools();
            
            createMaterialBuffer();

            createRayOutputImage();

            createBLAS();
            createTLAS();

            createRayDescriptorSet();
            createFullscreenDescriptorSet();

            createRayPipeline();
            createFullscreenPipeline();

            createShaderBindingTable();

            createFramebuffers();

            createCommandBuffers();
            createSyncObjects();
        }

        ~VulkanContext() {
            if (device != VK_NULL_HANDLE) {
                vkDeviceWaitIdle(device);

                cleanupRayTracing();
                cleanupFullscreen();
                cleanupAccelerationStructures();
                cleanupSyncObjects();
                cleanupSwapChain();
                cleanupPipelinesAndLayouts();
                cleanupCommandBuffersAndPool();

                vkDestroyDevice(device, nullptr);
            }

            cleanupSurfaceAndInstance();
        }

        void waitDeviceIdle() {
            vkDeviceWaitIdle(device);
        }
        // main.cpp functions

        // [Acquire Image] -> [Wait for graphics fence] -> [Draw Ray Frame] -> [Signal] 
        // -> [Draw Graphics Frame waiting on ray] -> [Present] -> [Increment Frame]

    void renderFrame() {
        FrameSync& sync = frameSync[currentFrame];

        std::cout << "=== Frame " << currentFrame << " start ===" << std::endl;

        VkResult inSyncWaitResult = vkWaitForFences(device, 1, &sync.inFlightFence, VK_TRUE, UINT64_MAX);
        std::cout << "inSyncWaitResult: " << inSyncWaitResult << std::endl;

        VkResult inSyncResetResult = vkResetFences(device, 1, &sync.inFlightFence);
        std::cout << "inSyncResetResult: " << inSyncResetResult << std::endl;

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, sync.imageAvailable, VK_NULL_HANDLE, &imageIndex);

        std::cout << "=== Frame " << currentFrame << ", acquired imageIndex: " << imageIndex << std::endl;

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            std::cout << "Swapchain out of date during vkAcquireNextImageKHR" << std::endl;
            // Recreate swapchain
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swapchain image!");
        }

        if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
            std::cout << "Waiting on fence for imageIndex " << imageIndex << std::endl;
            VkResult imagesInFlightWaitResult = vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
            std::cout << "imagesInFlightWaitResult: " << imagesInFlightWaitResult << std::endl;
        }
        imagesInFlight[imageIndex] = sync.inFlightFence;

        std::cout << "Resetting and recording command buffer for currentFrame " << currentFrame << std::endl;
        VkResult bufferResetResult = vkResetCommandBuffer(graphicsCommandBuffers[currentFrame], 0);
        std::cout << "bufferResetResult: " << bufferResetResult << std::endl;
        recordCommandBuffer(graphicsCommandBuffers[currentFrame], imageIndex);

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkSemaphore waitSemaphores[] = { sync.imageAvailable };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &graphicsCommandBuffers[currentFrame];
        VkSemaphore signalSemaphores[] = { sync.renderFinished };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        std::cout << "Submitting command buffer for frame " << currentFrame << std::endl;
        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, sync.inFlightFence) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit command buffer!");
        }

        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapChain;
        presentInfo.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(presentQueue, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
            std::cout << "Swapchain needs recreation during present" << std::endl;
            framebufferResized = false;
            // Recreate swapchain
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swapchain image!");
        }

        std::cout << "=== Frame " << currentFrame << " end ===" << std::endl;

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    private:
        VkSurfaceKHR surface = VK_NULL_HANDLE;

        // Swap chain related members
        VkSwapchainKHR swapChain = VK_NULL_HANDLE;
        VkFormat swapChainImageFormat = VK_FORMAT_UNDEFINED;
        VkExtent2D swapChainExtent = {};
        std::vector<VkImage> swapChainImages;
        std::vector<VkImageView> swapChainImageViews;
        
        const int MAX_FRAMES_IN_FLIGHT = 3;

        // Framebuffer related members
        VkRenderPass renderPass = VK_NULL_HANDLE;
        std::vector<VkFramebuffer> swapChainFramebuffers;

        VkPipelineLayout fullscreenPipelineLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout fullscreenDescriptorSetLayout = VK_NULL_HANDLE; 
        VkDescriptorPool fullscreenDescriptorPool = VK_NULL_HANDLE;
        VkDescriptorSet fullscreenDescriptorSet = VK_NULL_HANDLE;

        VkSampler sampler = VK_NULL_HANDLE;

        std::vector<VkPipeline> pipelines;

        // command buffers related members
        VkCommandPool graphicsCommandPool = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> graphicsCommandBuffers;

        // synchronization related members
        std::vector<FrameSync> frameSync;

        std::vector<VkFence> imagesInFlight;
        std::vector<VkSemaphore> renderFinishedSemaphores;

        size_t currentFrame = 0;

        // ray tracing related members
        VkImage rayOutputImage = VK_NULL_HANDLE;
        VkDeviceMemory rayOutputMemory = VK_NULL_HANDLE;
        VkImageView rayOutputImageView = VK_NULL_HANDLE;
        VkDescriptorSetLayout rayDescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool rayDescriptorPool = VK_NULL_HANDLE;
        VkDescriptorSet rayDescriptorSet = VK_NULL_HANDLE;
        VkStridedDeviceAddressRegionKHR raygenRegion {};
        VkStridedDeviceAddressRegionKHR missRegion {};
        VkStridedDeviceAddressRegionKHR hitRegion {};
        VkStridedDeviceAddressRegionKHR callableRegion{};
        VkBuffer raySBTBuffer = VK_NULL_HANDLE;
        VkDeviceMemory raySBTMemory = VK_NULL_HANDLE;
        VkPipelineLayout rayPipelineLayout = VK_NULL_HANDLE;

        VkBuffer vertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory vertexMemory = VK_NULL_HANDLE;

        VkBuffer materialBuffer = VK_NULL_HANDLE;
        VkDeviceMemory materialBufferMemory = VK_NULL_HANDLE;

        AccelerationStructure blas = {};
        AccelerationStructure tlas = {};


        bool framebufferResized = false;

        VkImageUsageFlags usage =
        VK_IMAGE_USAGE_STORAGE_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT;

        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
            QueueFamilyIndices indices;

            uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

            for (uint32_t i = 0; i < queueFamilyCount; i++) {
                const auto& props = queueFamilies[i];

                // Graphics
                if (props.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                    indices.graphicsFamily = i;
                }

                // Present
                VkBool32 presentSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
                if (presentSupport) {
                    indices.presentFamily = i;
                }

                if (indices.isComplete())
                    break;
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


        void validateSBTRegions(uint64_t groupCount, uint64_t baseAlignment, uint64_t sbtStride) {
            std::cout << "Validating SBT regions...\n";

            if (groupCount < 3) {
                throw std::runtime_error("Expected at least 3 shader groups: raygen, miss, hit");
            }

            // Sizes and strides must match sbtStride (aligned handle size)
            if (raygenRegion.size != sbtStride ||
                missRegion.size != sbtStride ||
                hitRegion.size != sbtStride) {
                throw std::runtime_error("SBT region sizes must match sbtStride!");
            }

            if (raygenRegion.stride != sbtStride ||
                missRegion.stride != sbtStride ||
                hitRegion.stride != sbtStride) {
                throw std::runtime_error("SBT region strides must match sbtStride!");
            }

            // Device addresses must be aligned to baseAlignment
            if ((raygenRegion.deviceAddress % baseAlignment) != 0 ||
                (missRegion.deviceAddress % baseAlignment) != 0 ||
                (hitRegion.deviceAddress % baseAlignment) != 0) {
                throw std::runtime_error("SBT region device addresses must be aligned to baseAlignment!");
            }

            // Sequential layout check using sbtStride (since you set deviceAddress with sbtStride increments)
            VkDeviceAddress base = raygenRegion.deviceAddress;
            if (missRegion.deviceAddress != base + sbtStride ||
                hitRegion.deviceAddress != base + 2 * sbtStride) {
                throw std::runtime_error("SBT region device addresses are not sequential as expected!");
            }

            std::cout << "SBT regions validated successfully (baseAlignment = " << baseAlignment
                    << ", sbtStride = " << sbtStride << ")\n";
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

            if (vkCreateRayTracingPipelinesKHR(device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipelines[PIPELINE_RAY_TRACING]) != VK_SUCCESS) {
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
            imageInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT; // Use a format suitable for ray tracing output
            imageInfo.extent.width = swapChainExtent.width;
            imageInfo.extent.height = swapChainExtent.height;
            imageInfo.extent.depth = 1;
            imageInfo.mipLevels = 1;
            imageInfo.arrayLayers = 1;
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT; // Storage for ray tracing output
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
            viewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT; // Same format as the image
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

            if (rayOutputImageView == VK_NULL_HANDLE) {
                std::cout << "Failed to create ray output image view" << std::endl;  
            }
        }

        void createRayDescriptorSetLayout() {
            std::vector<VkDescriptorSetLayoutBinding> bindings(3);

            // BINDING 0: Acceleration Structure
            bindings[0].binding = 0;
            bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
            bindings[0].descriptorCount = 1;
            bindings[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
            bindings[0].pImmutableSamplers = nullptr;

            // BINDING 1: Storage Image
            bindings[1].binding = 1;
            bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            bindings[1].descriptorCount = 1;
            bindings[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
            bindings[1].pImmutableSamplers = nullptr;

            // BINDING 1: Material Binding
            bindings[2].binding = 2;
            bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            bindings[2].descriptorCount = 1;
            bindings[2].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
            bindings[2].pImmutableSamplers = nullptr;

            VkDescriptorSetLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
            layoutInfo.pBindings = bindings.data();

            if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &rayDescriptorSetLayout) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create ray descriptor set layout!");
            }
        }

        void createRayDescriptorPool() {
            std::vector<VkDescriptorPoolSize> poolSizes(3);
            poolSizes[0].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
            poolSizes[0].descriptorCount = 1;
            poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            poolSizes[1].descriptorCount = 1;
            poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            poolSizes[2].descriptorCount = 1;

            VkDescriptorPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
            poolInfo.pPoolSizes = poolSizes.data();
            poolInfo.maxSets = 1; // Only one descriptor set

            if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &rayDescriptorPool) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create ray descriptor pool!");
            }
        }

        void createRayDescriptorSet() {
            // Allocate descriptor set
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = rayDescriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &rayDescriptorSetLayout;

            if (vkAllocateDescriptorSets(device, &allocInfo, &rayDescriptorSet) != VK_SUCCESS) {
                throw std::runtime_error("Failed to allocate ray descriptor set!");
            }

            // Acceleration Structure descriptor
            VkWriteDescriptorSetAccelerationStructureKHR accStructureInfo{};
            accStructureInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
            accStructureInfo.accelerationStructureCount = 1;
            accStructureInfo.pAccelerationStructures = &tlas.handle;

            VkWriteDescriptorSet accelWrite{};
            accelWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            accelWrite.dstSet = rayDescriptorSet;
            accelWrite.dstBinding = 0;
            accelWrite.dstArrayElement = 0;
            accelWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
            accelWrite.descriptorCount = 1;
            accelWrite.pNext = &accStructureInfo;

            // Storage image descriptor
            VkDescriptorImageInfo storageImageInfo{};
            storageImageInfo.imageView = rayOutputImageView;
            storageImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            VkWriteDescriptorSet imageWrite{};
            imageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            imageWrite.dstSet = rayDescriptorSet;
            imageWrite.dstBinding = 1;
            imageWrite.dstArrayElement = 0;
            imageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            imageWrite.descriptorCount = 1;
            imageWrite.pImageInfo = &storageImageInfo;

            // material buffer descriptor
            VkDescriptorBufferInfo materialBufferInfo{};
            materialBufferInfo.buffer = materialBuffer; // Your VkBuffer
            materialBufferInfo.offset = 0;
            materialBufferInfo.range = VK_WHOLE_SIZE;

            VkWriteDescriptorSet materialWrite{};
            materialWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            materialWrite.dstSet = rayDescriptorSet;
            materialWrite.dstBinding = 2; // Binding = 2 in your shader
            materialWrite.dstArrayElement = 0;
            materialWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            materialWrite.descriptorCount = 1;
            materialWrite.pBufferInfo = &materialBufferInfo;
            // Write both at once
            std::array<VkWriteDescriptorSet, 3> writes = { accelWrite, imageWrite, materialWrite };
            vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        }

        void createShaderBindingTable() {
            auto alignUp = [](VkDeviceAddress value, VkDeviceSize alignment) -> VkDeviceAddress {
                return (value + alignment - 1) & ~(alignment - 1);
            };

            // 1. Query SBT handle size and alignment
            VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProperties{};
            rayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

            VkPhysicalDeviceProperties2 deviceProperties2{};
            deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            deviceProperties2.pNext = &rayTracingProperties;
            vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProperties2);

            uint64_t handleSize = rayTracingProperties.shaderGroupHandleSize;
            uint64_t handleAlignment = rayTracingProperties.shaderGroupHandleAlignment; // For stride!
            uint64_t baseAlignment = rayTracingProperties.shaderGroupBaseAlignment;     // For deviceAddress!

            // 2. Calculate sizes with proper alignment
            uint32_t groupCount = 3;
            uint64_t handleSizeAligned = alignUp(handleSize, handleAlignment);
            uint64_t sbtStride = alignUp(handleSize, handleAlignment);
            sbtStride = alignUp(sbtStride, baseAlignment);
            uint64_t sbtSize = groupCount * sbtStride;
            // uint32_t sbtSize = groupCount * handleAlignment;

            // 3. Get shader group handles
            std::vector<uint8_t> shaderHandleStorage(static_cast<size_t>(groupCount * handleSize));
            if (vkGetRayTracingShaderGroupHandlesKHR(device, pipelines[PIPELINE_RAY_TRACING], 0, groupCount, groupCount * handleSize, shaderHandleStorage.data()) != VK_SUCCESS) {
                throw std::runtime_error("Failed to get shader group handles!");
            }

            // 4. Create SBT buffer
            createBuffer(
                sbtSize + baseAlignment,
                VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                raySBTBuffer,
                raySBTMemory,
                true // Use device address for SBT
            );

            // 5. Get device address and align it
            VkBufferDeviceAddressInfo addressInfo{};
            addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            addressInfo.buffer = raySBTBuffer;
            VkDeviceAddress sbtAddress = vkGetBufferDeviceAddress(device, &addressInfo);

            VkDeviceAddress alignedBase = alignUp(sbtAddress, baseAlignment);
            VkDeviceSize offset = alignedBase - sbtAddress;

            // 6. Copy handles into aligned buffer
            void* mappedData;
            vkMapMemory(device, raySBTMemory, 0, sbtSize + offset, 0, &mappedData);
            uint8_t* pData = reinterpret_cast<uint8_t*>(mappedData) + offset;
            for (uint32_t i = 0; i < groupCount; i++) {
                memcpy(pData + i * sbtStride, shaderHandleStorage.data() + i * handleSize, handleSize);
            }
            vkUnmapMemory(device, raySBTMemory);

            // 7. Fill in strided regions (used in vkCmdTraceRaysKHR)
            raygenRegion.deviceAddress = alignedBase;
            raygenRegion.stride = sbtStride;
            raygenRegion.size = sbtStride;

            missRegion.deviceAddress = alignedBase + sbtStride;
            missRegion.stride = sbtStride;
            missRegion.size = sbtStride;

            hitRegion.deviceAddress = alignedBase + 2 * sbtStride;
            hitRegion.stride = sbtStride;
            hitRegion.size = sbtStride;


            std::cout << "raygenRegion.deviceAddress = " << raygenRegion.deviceAddress << ", size = " << raygenRegion.size << ", stride = " << raygenRegion.stride << std::endl;
            std::cout << "missRegion.deviceAddress   = " << missRegion.deviceAddress << ", size = " << missRegion.size << ", stride = " << missRegion.stride << std::endl;
            std::cout << "hitRegion.deviceAddress    = " << hitRegion.deviceAddress << ", size = " << hitRegion.size << ", stride = " << hitRegion.stride << std::endl;


            // Optional: callable shaders
            callableRegion.deviceAddress = 0;
            callableRegion.stride = 0;
            callableRegion.size = 0;

            // Debug
            std::cout << "SBT base:       " << sbtAddress << std::endl;
            std::cout << "Aligned base:   " << alignedBase << std::endl;
            std::cout << "Handle stride:  " << handleSizeAligned << std::endl;
            std::cout << "SBT total size: " << sbtSize << std::endl;

            // Validate SBT regions
            validateSBTRegions(groupCount, baseAlignment, sbtStride);
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
            endSingleTimeCommands(cmd, graphicsQueue);

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
            endSingleTimeCommands(cmd, graphicsQueue);

            // Clean up scratch
            vkDestroyBuffer(device, scratchBuffer, nullptr);
            vkFreeMemory(device, scratchMemory, nullptr);

            vkDestroyBuffer(device, instanceBuffer, nullptr);
            vkFreeMemory(device, instanceMemory, nullptr);
        }

        VkCommandBuffer beginSingleTimeCommands() {
            VkCommandBufferAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandPool = graphicsCommandPool; // must be created with VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
            allocInfo.commandBufferCount = 1;

            VkCommandBuffer commandBuffer;

            if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
                throw std::runtime_error("Failed to allocate single time command buffer");
            }

            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

            if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
                throw std::runtime_error("Failed to begin single time command buffer");
            }

            return commandBuffer;
        }

        void endSingleTimeCommands(VkCommandBuffer commandBuffer, VkQueue queue) {
            vkEndCommandBuffer(commandBuffer);

            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;

            vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
            vkQueueWaitIdle(queue);

            vkFreeCommandBuffers(device, graphicsCommandPool, 1, &commandBuffer);
        }

        void createMaterialBuffer() {
            std::vector<Material> materialData = {
                // Lambert (diffuse red)
                { glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), glm::vec4(0.0f), glm::vec4(0.0f), MATERIAL_LAMBERT },

                // Metal (silver, rough)
                { glm::vec4(0.7f, 0.7f, 0.7f, 1.0f), glm::vec4(0.0f), glm::vec4(0.4f, 1.0f, 0.0f, 0.0f), MATERIAL_METAL },

                // Mirror (perfect reflector)
                { glm::vec4(1.0f), glm::vec4(0.0f), glm::vec4(0.0f), MATERIAL_MIRROR },

                // Dielectric (glass)
                { glm::vec4(1.0f), glm::vec4(0.0f), glm::vec4(0.0f, 0.0f, 1.5f, 0.0f), MATERIAL_DIELECTRIC },

                // Emissive (light source)
                { glm::vec4(1.0f), glm::vec4(5.0f, 5.0f, 5.0f, 1.0f), glm::vec4(0.0f), MATERIAL_EMISSIVE }
            };

            VkDeviceSize bufferSize = sizeof(Material) * materialData.size();

            createBuffer(
                bufferSize,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                materialBuffer,
                materialBufferMemory,
                true
            );

            void* data;
            vkMapMemory(device, materialBufferMemory, 0, bufferSize, 0, &data);
            memcpy(data, materialData.data(), static_cast<size_t>(bufferSize));
            vkUnmapMemory(device, materialBufferMemory);
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

        void createCommandPools() {
            createGraphicsCommandPool();
        }
        
        void createGraphicsCommandPool() {
            QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

            VkCommandPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // Optional
            poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
            
            if (vkCreateCommandPool(device, &poolInfo, nullptr, &graphicsCommandPool) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create command pool");
            }
        }
        
        // Helper to allocate command buffers from a given pool
        std::vector<VkCommandBuffer> allocateCommandBuffers(VkDevice device, VkCommandPool pool, uint32_t count) {
            std::vector<VkCommandBuffer> commandBuffers(count);

            VkCommandBufferAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandPool = pool;
            allocInfo.commandBufferCount = count;

            if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
                throw std::runtime_error("Failed to allocate command buffers");
            }
            return commandBuffers;
        }

        void createGraphicsCommandBuffers() {
            graphicsCommandBuffers = allocateCommandBuffers(device, graphicsCommandPool, MAX_FRAMES_IN_FLIGHT);
            for (size_t i = 0; i < graphicsCommandBuffers.size(); ++i) {
                std::cout << "Frame " << i << " - GFX: " << graphicsCommandBuffers[i] << std::endl;
            }
        }

        void createCommandBuffers() {
            createGraphicsCommandBuffers();
        }

        void createSyncObjects() {
            frameSync.resize(MAX_FRAMES_IN_FLIGHT);
            std::cout << "createSyncObjects() -> swapChainImages.size(): " << swapChainImages.size() << std::endl;
            imagesInFlight.resize(swapChainImages.size(), VK_NULL_HANDLE);

            VkSemaphoreCreateInfo semaphoreInfo{};
            semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

            VkFenceCreateInfo fenceInfo{};
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
                if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frameSync[i].imageAvailable) != VK_SUCCESS ||
                    vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frameSync[i].renderFinished) != VK_SUCCESS ||
                    vkCreateFence(device, &fenceInfo, nullptr, &frameSync[i].inFlightFence) != VK_SUCCESS)  {
                    throw std::runtime_error("Failed to create imageAvailable semaphore for frame " + std::to_string(i));
                }
            }
        }

        // main.cpp functions
        void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
            std::cout << "Recording command buffer for imageIndex: " << imageIndex << std::endl;

            vkResetCommandBuffer(commandBuffer, 0);

            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

            if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
                throw std::runtime_error("failed to begin recording command buffer!");
            }

            std::cout << "Transitioning ray output image to GENERAL layout" << std::endl;
            transitionImageLayout(commandBuffer, rayOutputImage,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

            std::cout << "Binding ray tracing pipeline and dispatching rays" << std::endl;
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelines[PIPELINE_RAY_TRACING]);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                rayPipelineLayout, 0, 1, &rayDescriptorSet, 0, nullptr);
            vkCmdTraceRaysKHR(commandBuffer, &raygenRegion, &missRegion, &hitRegion, &callableRegion,
                swapChainExtent.width, swapChainExtent.height, 1);

            std::cout << "Transitioning ray output image to SHADER_READ_ONLY_OPTIMAL layout" << std::endl;
            transitionImageLayout(commandBuffer, rayOutputImage,
                VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            std::cout << "Beginning render pass for final output, framebuffer imageIndex: " << imageIndex << std::endl;
            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = renderPass;
            renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
            renderPassInfo.renderArea.offset = {0, 0};
            renderPassInfo.renderArea.extent = swapChainExtent;

            VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
            renderPassInfo.clearValueCount = 1;
            renderPassInfo.pClearValues = &clearColor;

            vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            std::cout << "Binding fullscreen pipeline and drawing" << std::endl;
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[PIPELINE_FULLSCREEN]);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                fullscreenPipelineLayout, 0, 1, &fullscreenDescriptorSet, 0, nullptr);

            vkCmdDraw(commandBuffer, 3, 1, 0, 0);

            vkCmdEndRenderPass(commandBuffer);

            if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
                throw std::runtime_error("failed to end recording command buffer!");
            }

            std::cout << "Finished recording command buffer for imageIndex: " << imageIndex << std::endl;
        }
        
        void recordRayTracingCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; 
            beginInfo.pInheritanceInfo = nullptr; // Optional

            VkResult bresult = vkBeginCommandBuffer(commandBuffer, &beginInfo);
            if (bresult != VK_SUCCESS) {
                std::cerr << "rAY BeginCommandBuffer failed with error: " << bresult << std::endl;
                throw std::runtime_error("failed to begin recording ray command buffer!");
            }

            transitionImageLayout(commandBuffer, rayOutputImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelines[PIPELINE_RAY_TRACING]);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rayPipelineLayout, 0, 1, &rayDescriptorSet, 0, nullptr);

            vkCmdTraceRaysKHR(commandBuffer, &raygenRegion, &missRegion, &hitRegion, &callableRegion, swapChainExtent.width, swapChainExtent.height, 1);

            transitionImageLayout(commandBuffer, rayOutputImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            // if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            //     throw std::runtime_error("failed to end ray record command buffer!");
            // }

            VkResult result = vkEndCommandBuffer(commandBuffer);
            if (result != VK_SUCCESS) {
                std::cerr << "rAY endCommandBuffer failed with error: " << result << std::endl;
                throw std::runtime_error("Failed to submit command buffer!");
            }
        }

        void recordGraphicsCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // Optional
            beginInfo.pInheritanceInfo = nullptr; // Optional

            if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
                throw std::runtime_error("failed to begin recording graphics command buffer!");
            }

            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = renderPass;
            renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
            renderPassInfo.renderArea.offset = {0, 0};
            renderPassInfo.renderArea.extent = swapChainExtent;

            VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
            renderPassInfo.clearValueCount = 1;
            renderPassInfo.pClearValues = &clearColor;

            vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[PIPELINE_FULLSCREEN]);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, fullscreenPipelineLayout, 0, 1, &fullscreenDescriptorSet, 0, nullptr);
            
            vkCmdDraw(commandBuffer, 3, 1, 0, 0);

            vkCmdEndRenderPass(commandBuffer);
            
            VkResult result = vkEndCommandBuffer(commandBuffer);
            if (result != VK_SUCCESS) {
                std::cerr << "GRAPHICS endCommandBuffer failed with error: " << result << std::endl;
                throw std::runtime_error("Failed to submit command buffer!");
            }
            // if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            //     throw std::runtime_error("failed to end graphics record command buffer!");
            // }
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

            LayoutTransition transitionLayout = getLayoutTransition(oldLayout, newLayout);

            barrier.srcAccessMask = transitionLayout.srcAccess;
            barrier.dstAccessMask = transitionLayout.dstAccess;

            vkCmdPipelineBarrier(
                commandBuffer,
                transitionLayout.srcStage, transitionLayout.dstStage,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );
        }

        LayoutTransition getLayoutTransition(VkImageLayout oldLayout, VkImageLayout newLayout) {
            if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
                return {0, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR};
            }
            else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
                return {VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT};
            }
            else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
                return {0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT};
            }
            else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
                return {VK_ACCESS_TRANSFER_WRITE_BIT, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT};
            }
            else if (oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
                return {0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT};
            }
            else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
                return {VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};
            }
            else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
                return {VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR};
            }
            else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
                return {VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT};
            }
            else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
                return {0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
            }
            else if (oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
                return {0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
            }
            else {
                throw std::invalid_argument("unsupported layout transition!");
            }
        }


        // this function is for the fullscree
        void createFullscreenPipeline() {
            auto vertShaderCode = readFile("shaders/fullscreen/vert.spv");
            auto fragShaderCode = readFile("shaders/fullscreen/frag.spv");

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

            VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

            VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
            vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

            VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
            inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            inputAssembly.primitiveRestartEnable = VK_FALSE;

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(swapChainExtent.width);
            viewport.height = static_cast<float>(swapChainExtent.height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;

            VkRect2D scissor{};
            scissor.offset = { 0, 0 };
            scissor.extent = swapChainExtent;

            VkPipelineViewportStateCreateInfo viewportState{};
            viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewportState.viewportCount = 1;
            viewportState.pViewports = &viewport;
            viewportState.scissorCount = 1;
            viewportState.pScissors = &scissor;

            VkPipelineRasterizationStateCreateInfo rasterizer{};
            rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rasterizer.depthClampEnable = VK_FALSE;
            rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
            rasterizer.lineWidth = 1.0f;
            rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
            rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

            VkPipelineMultisampleStateCreateInfo multisampling{};
            multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

            VkPipelineColorBlendAttachmentState colorBlendAttachment{};
            colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

            VkPipelineColorBlendStateCreateInfo colorBlending{};
            colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlending.logicOpEnable = VK_FALSE;
            colorBlending.logicOp = VK_LOGIC_OP_COPY;   
            colorBlending.attachmentCount = 1;
            colorBlending.pAttachments = &colorBlendAttachment;

            VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = 1;
            pipelineLayoutInfo.pSetLayouts = &fullscreenDescriptorSetLayout;

            if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &fullscreenPipelineLayout) != VK_SUCCESS)
                throw std::runtime_error("Failed to create pipeline layout!");

            VkGraphicsPipelineCreateInfo pipelineInfo{};
            pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipelineInfo.stageCount = 2;
            pipelineInfo.pStages = shaderStages;
            pipelineInfo.pVertexInputState = &vertexInputInfo;
            pipelineInfo.pInputAssemblyState = &inputAssembly;
            pipelineInfo.pViewportState = &viewportState;
            pipelineInfo.pRasterizationState = &rasterizer;
            pipelineInfo.pMultisampleState = &multisampling;
            pipelineInfo.pColorBlendState = &colorBlending;
            pipelineInfo.layout = fullscreenPipelineLayout;
            pipelineInfo.renderPass = renderPass;
            pipelineInfo.subpass = 0;
            pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
            pipelineInfo.basePipelineIndex = -1;

            if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipelines[PIPELINE_FULLSCREEN]) != VK_SUCCESS)
                throw std::runtime_error("Failed to create graphics pipeline!");

            // Cleanup
            vkDestroyShaderModule(device, fragShaderModule, nullptr);
            vkDestroyShaderModule(device, vertShaderModule, nullptr);
        }

        void createFullscreenDescriptorSetLayout() {
            VkDescriptorSetLayoutBinding samplerBinding{};
            samplerBinding.binding = 0;
            samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            samplerBinding.descriptorCount = 1;
            samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            samplerBinding.pImmutableSamplers = nullptr;

            VkDescriptorSetLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutInfo.bindingCount = 1;
            layoutInfo.pBindings = &samplerBinding;

            if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &fullscreenDescriptorSetLayout) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create fullscreen descriptor set layout!");
            }
        }

        void createFullscreenDescriptorSet() {
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = fullscreenDescriptorPool; // Ensure this pool exists
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &fullscreenDescriptorSetLayout;

            if (vkAllocateDescriptorSets(device, &allocInfo, &fullscreenDescriptorSet) != VK_SUCCESS) {
                throw std::runtime_error("Failed to allocate fullscreen descriptor set!");
            }

            VkDescriptorImageInfo imageInfo{};
            // imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            imageInfo.imageLayout =  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = rayOutputImageView;  // from ray tracing pass
            imageInfo.sampler = sampler;

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = fullscreenDescriptorSet;
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pImageInfo = &imageInfo;

            vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
        }

        void createFullscreenDescriptorPool() {
            VkDescriptorPoolSize poolSize{};
            poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            poolSize.descriptorCount = 1;

            VkDescriptorPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            poolInfo.poolSizeCount = 1;
            poolInfo.pPoolSizes = &poolSize;
            poolInfo.maxSets = 1;

            if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &fullscreenDescriptorPool) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create fullscreen descriptor pool!");
            }
        }

        void createSampler() {
            VkSamplerCreateInfo samplerInfo{};
            samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.magFilter = VK_FILTER_LINEAR;
            samplerInfo.minFilter = VK_FILTER_LINEAR;
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.anisotropyEnable = VK_FALSE;
            samplerInfo.maxAnisotropy = 1.0f;

            if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
                throw std::runtime_error("failed to create sampler!");
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
        }

        void cleanupRayTracing() {
            if (rayOutputImageView != VK_NULL_HANDLE) {
                vkDestroyImageView(device, rayOutputImageView, nullptr);
                rayOutputImageView = VK_NULL_HANDLE;
            }
            if (rayOutputImage != VK_NULL_HANDLE) {
                vkDestroyImage(device, rayOutputImage, nullptr);
                rayOutputImage = VK_NULL_HANDLE;
            }
            if (rayOutputMemory != VK_NULL_HANDLE) {
                vkFreeMemory(device, rayOutputMemory, nullptr);
                rayOutputMemory = VK_NULL_HANDLE;
            }
            if (rayDescriptorSetLayout != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(device, rayDescriptorSetLayout, nullptr);
                rayDescriptorSetLayout = VK_NULL_HANDLE;
            }
            if (rayDescriptorPool != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(device, rayDescriptorPool, nullptr);
                rayDescriptorPool = VK_NULL_HANDLE;
            }
            if (raySBTBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, raySBTBuffer, nullptr);
                raySBTBuffer = VK_NULL_HANDLE;
            }
            if (raySBTMemory != VK_NULL_HANDLE) {
                vkFreeMemory(device, raySBTMemory, nullptr);
                raySBTMemory = VK_NULL_HANDLE;
            }
            if (rayPipelineLayout != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(device, rayPipelineLayout, nullptr);
                rayPipelineLayout = VK_NULL_HANDLE;
            }
        }

        void cleanupFullscreen() {

            if (sampler != VK_NULL_HANDLE) {
                vkDestroySampler(device, sampler, nullptr);
                sampler = VK_NULL_HANDLE;
            }
            if (fullscreenDescriptorSetLayout != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(device, fullscreenDescriptorSetLayout, nullptr);
                fullscreenDescriptorSetLayout = VK_NULL_HANDLE;
            }
            if (fullscreenDescriptorPool != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(device, fullscreenDescriptorPool, nullptr);
                fullscreenDescriptorPool = VK_NULL_HANDLE;
            }

            if (fullscreenPipelineLayout != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(device, fullscreenPipelineLayout, nullptr);
                fullscreenPipelineLayout = VK_NULL_HANDLE;
            }
        }

        void cleanupAccelerationStructures() {
            if (materialBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, materialBuffer, nullptr);
                materialBuffer = VK_NULL_HANDLE;
            }

            if (materialBufferMemory != VK_NULL_HANDLE) {
                vkFreeMemory(device, materialBufferMemory, nullptr);
                materialBufferMemory = VK_NULL_HANDLE;
            }

            if (vertexBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, vertexBuffer, nullptr);
                vertexBuffer = VK_NULL_HANDLE;
            }

            if (vertexMemory != VK_NULL_HANDLE) {
                vkFreeMemory(device, vertexMemory, nullptr);
                vertexMemory = VK_NULL_HANDLE;
            }

            // BLAS
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
            blas.deviceAddress = 0;
            // TLAS
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
            tlas.deviceAddress = 0;
        }

        void cleanupSyncObjects() {
            for (const auto& sync : frameSync) {
                vkDestroySemaphore(device, sync.imageAvailable, nullptr);
                // vkDestroySemaphore(device, sync.rayFinished, nullptr);
                vkDestroySemaphore(device, sync.renderFinished, nullptr);
                vkDestroyFence(device, sync.inFlightFence, nullptr);
            }
            imagesInFlight.clear();
        }

        void cleanupSwapChain() {
            for (auto framebuffer : swapChainFramebuffers) {
                if (framebuffer != VK_NULL_HANDLE)
                    vkDestroyFramebuffer(device, framebuffer, nullptr);
            }
            swapChainFramebuffers.clear();

            for (auto imageView : swapChainImageViews) {
                if (imageView != VK_NULL_HANDLE)
                    vkDestroyImageView(device, imageView, nullptr);
            }
            swapChainImageViews.clear();

            if (swapChain != VK_NULL_HANDLE) {
                vkDestroySwapchainKHR(device, swapChain, nullptr);
                swapChain = VK_NULL_HANDLE;
            }

            if (renderPass != VK_NULL_HANDLE) {
                vkDestroyRenderPass(device, renderPass, nullptr);
                renderPass = VK_NULL_HANDLE;
            }
        }

        void cleanupPipelinesAndLayouts() {
            for (auto pipeline : pipelines) {
                if (pipeline != VK_NULL_HANDLE)
                    vkDestroyPipeline(device, pipeline, nullptr);
            }
            pipelines.clear();
        }

        void cleanupCommandBuffersAndPool() {
            if (!graphicsCommandBuffers.empty()) {
                vkFreeCommandBuffers(device, graphicsCommandPool, static_cast<uint32_t>(graphicsCommandBuffers.size()), graphicsCommandBuffers.data());
                graphicsCommandBuffers.clear();
            }

            if (graphicsCommandPool != VK_NULL_HANDLE) {
                vkDestroyCommandPool(device, graphicsCommandPool, nullptr);
                graphicsCommandPool = VK_NULL_HANDLE;
            }
        }

        void cleanupSurfaceAndInstance() {
            if (surface != VK_NULL_HANDLE) {
                vkDestroySurfaceKHR(instance, surface, nullptr);
                surface = VK_NULL_HANDLE;
            }

            if (enableValidationLayers && debugMessenger != VK_NULL_HANDLE) {
                DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
                debugMessenger = VK_NULL_HANDLE;
            }

            if (instance != VK_NULL_HANDLE) {
                vkDestroyInstance(instance, nullptr);
                instance = VK_NULL_HANDLE;
            }
        }

};

#endif