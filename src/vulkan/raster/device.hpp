#pragma once

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>
#include <string>
#include <stdexcept>
#include <optional>
#include <set>

#include "instance.hpp"

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value(); // extend if needed
    }
};

class VulkanDevice{
    public: 
        VulkanDevice(
            VulkanInstance instance, 
            VkSurfaceKHR surface,
            const std::vector<const char*>& requiredExtensions,
            const VkPhysicalDeviceFeatures& deviceFeatures,
            const void* nextDeviceFeatures
        ){
            pickPhysicalDevice(instance.getInstance(), surface, requiredExtensions);
            createLogicalDevice(
                instance,
                requiredExtensions,
                deviceFeatures,
                nextDeviceFeatures
            );
        }

        ~VulkanDevice() {
            if (device != nullptr) {
                vkDestroyDevice(device, nullptr);
                device = nullptr;
            }
        }

        void wait() const {
            std::cout << "-- Start: vkDeviceWaitIdle -- " << std::endl;
            vkDeviceWaitIdle(device);
            std::cout << "-- End: vkDeviceWaitIdle -- " << std::endl;
        }

        const VkDevice& getDevice() const {
            return device;
        }

        const VkQueue& getGraphicsQueue() const {
            return graphicsQueue;
        }

        const VkQueue& getPresentQueue() const {
            return presentQueue;
        }

        const VkPhysicalDevice& getPhysicalDevice() const {
            return physicalDevice;
        }

        const uint32_t getGraphicsFamilyIndex() const {
            return graphicsFamilyIndex;
        }

        const uint32_t getPresentFamilyIndex() const {
            return presentFamilyIndex;
        }

    private:
        VkDevice device = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkQueue graphicsQueue = VK_NULL_HANDLE;
        VkQueue presentQueue = VK_NULL_HANDLE;
        // VkQueue computeQueue = VK_NULL_HANDLE;

        uint32_t graphicsFamilyIndex {};
        uint32_t presentFamilyIndex {};

        // const std::vector<const char*> deviceExtensions = {
        //     VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        //     VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        //     VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        //     VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        //     VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        //     VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        //     VK_KHR_SPIRV_1_4_EXTENSION_NAME,
        //     VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME
        // };

        void pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, const std::vector<const char*>& deviceExtensions) {
            uint32_t deviceCount = 0;
            vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

            if (deviceCount == 0) {
                throw std::runtime_error("Failed to find GPUs with Vulkan support");
            }

            std::vector<VkPhysicalDevice> devices(deviceCount);
            vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

            for (const auto& device : devices) {
                if (isDeviceSuitable(device, surface, deviceExtensions)) {
                    physicalDevice = device;
                    break;
                }
            }

            if (physicalDevice == VK_NULL_HANDLE) {
                throw std::runtime_error("Failed to find a suitable GPU");
            }
        }

        void createLogicalDevice(
            VulkanInstance instance, 
            const std::vector<const char*>& requiredExtensions,
            const VkPhysicalDeviceFeatures& deviceFeatures,
            const void* nextDeviceFeatures
        ) {
            // QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

            // std::set<uint32_t> uniqueFamilies = {
            //     indices.graphicsFamily.value(),
            //     indices.presentFamily.value()
            // };

            std::set<uint32_t> uniqueFamilies = {
                graphicsFamilyIndex,
                presentFamilyIndex
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
            
            VkDeviceCreateInfo deviceInfo{};
            deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

            deviceInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
            deviceInfo.pQueueCreateInfos = queueCreateInfos.data();

            deviceInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
            deviceInfo.ppEnabledExtensionNames = requiredExtensions.data();

            deviceInfo.enabledLayerCount = static_cast<uint32_t>(instance.getValidationLayers().size());
            deviceInfo.ppEnabledLayerNames = instance.getValidationLayers().data();

            deviceInfo.pEnabledFeatures = &deviceFeatures;
            deviceInfo.pNext = nextDeviceFeatures;

            if (vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create logical device");
            }

            vkGetDeviceQueue(device, graphicsFamilyIndex, 0, &graphicsQueue);
            vkGetDeviceQueue(device, presentFamilyIndex, 0, &presentQueue);
        }

        bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface, const std::vector<const char*>& deviceExtensions) {
            QueueFamilyIndices indices = findQueueFamilies(device, surface);

            if (indices.graphicsFamily.has_value()) {
                graphicsFamilyIndex = indices.graphicsFamily.value();
            }

            if (indices.presentFamily.has_value()) {
                presentFamilyIndex = indices.presentFamily.value();
            }

            bool extensionsSupported = checkDeviceExtensionSupport(device, deviceExtensions);

            // bool swapChainAdequate = false;
            // if (extensionsSupported) {
            //     SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device, surface);
            //     swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
            // }

            return indices.isComplete() && extensionsSupported;
        }

        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
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
        
        bool checkDeviceExtensionSupport(VkPhysicalDevice device, const std::vector<const char*>& deviceExtensions) {
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
};