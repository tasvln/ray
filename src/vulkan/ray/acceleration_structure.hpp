#pragma once

#include "vulkan/raster/device.hpp"
#include "vulkan/raster/buffer.hpp"
#include "dispatch_table.hpp"
#include "device_properties.hpp"
#include "utils/acceleration_structure.hpp"

class VulkanRayAccelerationStructure {
    public:
        VulkanRayAccelerationStructure(
            const VulkanDevice& device,
            const VulkanRayDispatchTable& dispatch,
            const VulkanRayDeviceProperties& rayDeviceProperties,
            VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
        ):  device(device),
            dispatch(dispatch),
            rayDeviceProperties(rayDeviceProperties),
            flags(flags)
        {}

        VulkanRayAccelerationStructure(
            VulkanRayAccelerationStructure&& other
        ) noexcept : 
            device(other.device),
            dispatch(other.dispatch),
            rayDeviceProperties(other.rayDeviceProperties),
            flags(other.flags),
            structure(other.structure),
            buildSizeInfo(other.buildSizeInfo),
            buildGeometryInfo(other.buildGeometryInfo)
        {
		    other.structure = VK_NULL_HANDLE;
	    }

        virtual ~VulkanRayAccelerationStructure() {
            if (structure) {
                dispatch.vkDestroyAccelerationStructureKHR(device.getDevice(), structure, nullptr);
                structure = VK_NULL_HANDLE;
            }
        }

        void createStructure(VulkanBuffer& buffer, const VkDeviceSize offset) {
            VkAccelerationStructureCreateInfoKHR createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
            createInfo.pNext = nullptr;
            createInfo.type = buildGeometryInfo.type;
	        createInfo.size = buildSizeInfo.accelerationStructureSize;
            createInfo.buffer = buffer.getBuffer();
            createInfo.offset = offset;

            if(dispatch.vkCreateAccelerationStructureKHR(device.getDevice(),  &createInfo, nullptr, &structure) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create acceleration structure -> VulkanRayAccelerationStructure()");
            }
        }

        void memoryBarrier(VkCommandBuffer commandBuffer) {
            // hmm -> Wait for the builder to complete by setting a barrier on the resulting buffer
            // aparently ->  important as the construction of the top-level hierarchy may be called right afterwards, before executing the command list

            VkMemoryBarrier memoryBarrier = {};
            memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            memoryBarrier.pNext = nullptr;
            memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
	        memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                0, 
                1, 
                &memoryBarrier, 
                0, 
                nullptr, 
                0, 
                nullptr
            );
        }

        VkAccelerationStructureBuildSizesInfoKHR getBuildSizes(const uint32_t* maxPrimitiveCounts) {
            // Query both the size of the finished acceleration structure and the amount of scratch memory needed.
            VkAccelerationStructureBuildSizesInfoKHR sizesInfo{};
            sizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

            dispatch.vkGetAccelerationStructureBuildSizesKHR(
                device.getDevice(),
                VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                &buildGeometryInfo,
                maxPrimitiveCounts,
                &sizesInfo
            );

            // AccelerationStructure offset needs to be 256 bytes aligned (official Vulkan specs, don't ask me why).
            constexpr uint64_t accelerationStructureAlignment = 256;
            const uint64_t scratchAlignment = rayDeviceProperties.getMinAccelerationStructureScratchOffsetAlignment();

            sizesInfo.accelerationStructureSize = utils::alignUp(sizesInfo.accelerationStructureSize, accelerationStructureAlignment);
            sizesInfo.buildScratchSize = utils::alignUp(sizesInfo.buildScratchSize, scratchAlignment);

            return sizesInfo;
        }

        const VkAccelerationStructureKHR& getStructure() const {
            return structure;
        }

        const VkAccelerationStructureBuildSizesInfoKHR& getBuildSizeInfo() const {
            return buildSizeInfo;
        }
        
        const VkAccelerationStructureBuildGeometryInfoKHR& getBuildGeometryInfo() const {
            return buildGeometryInfo;
        }

        const VulkanRayDispatchTable& getDispatchTable() const {
            return dispatch;
        }

    protected:
        const VulkanDevice device;
        const VulkanRayDispatchTable dispatch;
        
        VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo{};
        VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo{};
        VkBuildAccelerationStructureFlagsKHR flags;
        
    private:
        VkAccelerationStructureKHR structure = VK_NULL_HANDLE;
        const VulkanRayDeviceProperties rayDeviceProperties;
        
};