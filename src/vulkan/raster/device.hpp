#pragma once

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>
#include <string>
#include <stdexcept>
#include <optional>
#include <set>

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value(); // extend if needed
    }
};

class VulkanDevice{
    public: 
        VulkanDevice(VkInstance instance, VkSurfaceKHR surface){
            pickPhysicalDevice(instance, surface);
            createLogicalDevice();
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

        void pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface) {
            uint32_t deviceCount = 0;
            vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

            if (deviceCount == 0) {
                throw std::runtime_error("Failed to find GPUs with Vulkan support");
            }

            std::vector<VkPhysicalDevice> devices(deviceCount);
            vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

            for (const auto& device : devices) {
                if (isDeviceSuitable(device, surface)) {
                    physicalDevice = device;
                    break;
                }
            }

            if (physicalDevice == VK_NULL_HANDLE) {
                throw std::runtime_error("Failed to find a suitable GPU");
            }
        }

        void createLogicalDevice() {
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

            // 1. Descriptor indexing features
            VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures{};
            descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
            descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
            descriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;
            descriptorIndexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;
            descriptorIndexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;

            // 2. Acceleration structure features
            VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{};
            accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
            accelerationStructureFeatures.accelerationStructure = VK_TRUE;

            // 3. Ray tracing pipeline features
            VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures{};
            rayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
            rayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;

            // 4. Buffer device address features
            VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{};
            bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
            bufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;

            // 5. Chain pNext pointers properly:
            // Descriptor indexing -> Acceleration Structure -> Ray Tracing Pipeline -> Buffer Device Address
            descriptorIndexingFeatures.pNext = &accelerationStructureFeatures;
            accelerationStructureFeatures.pNext = &rayTracingPipelineFeatures;
            rayTracingPipelineFeatures.pNext = &bufferDeviceAddressFeatures;
            bufferDeviceAddressFeatures.pNext = nullptr;

            // 6. Base features struct (could also enable some core features here if needed)
            VkPhysicalDeviceFeatures2 deviceFeatures2{};
            deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            deviceFeatures2.pNext = &descriptorIndexingFeatures;

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

            vkGetDeviceQueue(device, graphicsFamilyIndex, 0, &graphicsQueue);
            vkGetDeviceQueue(device, presentFamilyIndex, 0, &presentQueue);
        }

        bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface) {
            QueueFamilyIndices indices = findQueueFamilies(device, surface);

            if (indices.graphicsFamily.has_value()) {
                graphicsFamilyIndex = indices.graphicsFamily.value();
            }

            if (indices.presentFamily.has_value()) {
                presentFamilyIndex = indices.presentFamily.value();
            }

            bool extensionsSupported = checkDeviceExtensionSupport(device);

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
};