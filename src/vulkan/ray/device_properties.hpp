#pragma once

#include <vulkan/vulkan.hpp>
#include "vulkan/device.hpp"

class VulkanRayDeviceProperties {
    public:
        VulkanRayDeviceProperties(
            const VulkanDevice device
        ) {
            // didn't do this in my context class
            accelerationProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
            
            pipelineProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
            pipelineProps.pNext = &accelerationProps;

            
            VkPhysicalDeviceProperties2 deviceProps{};
            deviceProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            deviceProps.pNext = &pipelineProps;
            vkGetPhysicalDeviceProperties2(device.getPhysicalDevice(), &deviceProps);
        }

        ~VulkanRayDeviceProperties() = default;

        // acceleration props

        uint32_t getMaxDescriptorSetAccelerationStructures() const { 
            return accelerationProps.maxDescriptorSetAccelerationStructures; 
        }

        uint64_t getMaxGeometryCount() const { 
            return accelerationProps.maxGeometryCount; 
        }

        uint64_t getMaxInstanceCount() const { 
            return accelerationProps.maxInstanceCount; 
        }

        uint64_t getMaxPrimitiveCount() const { 
            return accelerationProps.maxPrimitiveCount; 
        }

        // pipeline props

        uint32_t getMaxRayRecursionDepth() const { 
            return pipelineProps.maxRayRecursionDepth; 
        }

        uint32_t getMaxShaderGroupStride() const { 
            return pipelineProps.maxShaderGroupStride; 
        }

        uint32_t getMinAccelerationStructureScratchOffsetAlignment() const { 
            return accelerationProps.minAccelerationStructureScratchOffsetAlignment; 
        }

        uint32_t getShaderGroupBaseAlignment() const { 
            return pipelineProps.shaderGroupBaseAlignment; 
        }

        uint32_t getShaderGroupHandleCaptureReplaySize() const { 
            return pipelineProps.shaderGroupHandleCaptureReplaySize; 
        }

        uint32_t getShaderGroupHandleSize() const { 
            return pipelineProps.shaderGroupHandleSize; 
        }


    private:
        VkPhysicalDeviceAccelerationStructurePropertiesKHR accelerationProps{};
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR pipelineProps{};
};