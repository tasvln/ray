#pragma once

#include <vulkan/vulkan.hpp>
#include "vulkan/device.hpp"

class VulkanRayDispatchTable {
    public: 
        VulkanRayDispatchTable(
            const VulkanDevice& device
        ) {
            loadFunctions(device);
        }

        ~VulkanRayDispatchTable() = default;

        // Vulkan Ray Tracing Function Pointers
        PFN_vkCreateAccelerationStructureKHR                vkCreateAccelerationStructureKHR = nullptr;
        PFN_vkDestroyAccelerationStructureKHR               vkDestroyAccelerationStructureKHR = nullptr;
        PFN_vkGetAccelerationStructureBuildSizesKHR         vkGetAccelerationStructureBuildSizesKHR = nullptr;
        PFN_vkCmdBuildAccelerationStructuresKHR             vkCmdBuildAccelerationStructuresKHR = nullptr;
        PFN_vkCmdCopyAccelerationStructureKHR               vkCmdCopyAccelerationStructureKHR = nullptr;
        PFN_vkCmdTraceRaysKHR                               vkCmdTraceRaysKHR = nullptr;
        PFN_vkCreateRayTracingPipelinesKHR                  vkCreateRayTracingPipelinesKHR = nullptr;
        PFN_vkGetRayTracingShaderGroupHandlesKHR            vkGetRayTracingShaderGroupHandlesKHR = nullptr;
        PFN_vkGetAccelerationStructureDeviceAddressKHR      vkGetAccelerationStructureDeviceAddressKHR = nullptr;
        PFN_vkCmdWriteAccelerationStructuresPropertiesKHR   vkCmdWriteAccelerationStructuresPropertiesKHR = nullptr;

    private:
        template <typename T>
        T loadProc(VkDevice device, const char* name) {
            T func = reinterpret_cast<T>(vkGetDeviceProcAddr(device, name));
            if (!func) {
                throw std::runtime_error(std::string("Failed to load Vulkan function: ") + name);
            }
            return func;
        }

        void loadFunctions(const VulkanDevice device) {
            #define LOAD_PROC(name) name = loadProc<decltype(name)>(device.getDevice(), #name)

                LOAD_PROC(vkCreateAccelerationStructureKHR);
                LOAD_PROC(vkDestroyAccelerationStructureKHR);
                LOAD_PROC(vkGetAccelerationStructureBuildSizesKHR);
                LOAD_PROC(vkCmdBuildAccelerationStructuresKHR);
                LOAD_PROC(vkCmdCopyAccelerationStructureKHR);
                LOAD_PROC(vkCmdTraceRaysKHR);
                LOAD_PROC(vkCreateRayTracingPipelinesKHR);
                LOAD_PROC(vkGetRayTracingShaderGroupHandlesKHR);
                LOAD_PROC(vkGetAccelerationStructureDeviceAddressKHR);
                LOAD_PROC(vkCmdWriteAccelerationStructuresPropertiesKHR);

            #undef LOAD_PROC
        }
};