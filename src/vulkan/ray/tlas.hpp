#pragma once

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <vector>

#include "blas.hpp"
#include "acceleration_structure.hpp"

class VulkanRayTLAS : public VulkanRayAccelerationStructure {
    public:
        VulkanRayTLAS(
            const VulkanDevice& device,
            const VulkanRayDispatchTable& dispatch,
            const VulkanRayDeviceProperties& rayDeviceProperties,
            const VkDeviceAddress addr,
            const uint32_t count
        ):  VulkanRayAccelerationStructure(device, dispatch, rayDeviceProperties),
            tlasInstanceCount(tlasInstanceCount) 
        {
            createGeometry(addr, count);
        }

        VulkanRayTLAS(VulkanRayTLAS&& src) :
            VulkanRayAccelerationStructure(std::move(src)),
            tlasInstanceCount(std::move(src.tlasInstanceCount)) 
        {}

        ~VulkanRayTLAS() = default;

        VkAccelerationStructureInstanceKHR createTLASInstance(
            const VulkanRayBLAS& blas,
            const glm::mat4& transform,
            const uint32_t instanceId,
            const uint32_t hitGroupId
        ) {
            VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
            addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
            addressInfo.accelerationStructure = blas.getStructure();

            const VkDeviceAddress addr = blas.getDispatchTable().vkGetAccelerationStructureDeviceAddressKHR(device.getDevice(), &addressInfo);
        
            VkAccelerationStructureInstanceKHR instance{};
            instance.instanceCustomIndex = instanceId;
            instance.mask = 0xFF;
            // Set the hit group index, that will be used to find the shader code to execute when hitting the geometry.
            instance.instanceShaderBindingTableRecordOffset = hitGroupId;
            instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            instance.accelerationStructureReference = addr;

            if (sizeof(instance.transform) == sizeof(float) * 12) {
                std::cout << "transform size mismatch" << std::endl;
            }

            std::memcpy(&instance.transform, &transform, sizeof(instance.transform));

            return instance;
        }

        void generateTLAS(
            VkCommandBuffer commandBuffer,
            VulkanBuffer& buffer,
            const VkDeviceSize offset,
            VulkanBuffer& scratchBuffer,
            const VkDeviceSize scratchOffset
        ) {
            createStructure(buffer, offset);

            // Build the actual bottom-level acceleration structure
            VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
            buildRangeInfo.primitiveCount = tlasInstanceCount;

            const VkAccelerationStructureBuildRangeInfoKHR* structureBuildRange = &buildRangeInfo;

            buildGeometryInfo.dstAccelerationStructure = getStructure();
            buildGeometryInfo.scratchData.deviceAddress = scratchBuffer.getDeviceAddress() + scratchOffset;

            dispatch.vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildGeometryInfo, &structureBuildRange);
        }   

        void createGeometry(const VkDeviceAddress addr, const uint32_t count) {
            tlasGeometryInstances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
            tlasGeometryInstances.arrayOfPointers = VK_FALSE;
            tlasGeometryInstances.data.deviceAddress = addr;

            tlasGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
            tlasGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
            tlasGeometry.geometry.instances = tlasGeometryInstances;

            buildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
            buildGeometryInfo.flags = flags;
            buildGeometryInfo.geometryCount = 1;
            buildGeometryInfo.pGeometries = &tlasGeometry;
            buildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
            buildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
            buildGeometryInfo.srcAccelerationStructure = nullptr;

            buildSizeInfo = getBuildSizes(&count);
        }

        uint32_t getInstanceCount() const {
            return tlasInstanceCount;
        }

        const VkAccelerationStructureGeometryInstancesDataKHR& getTLASGeometryInstances() const {
            return tlasGeometryInstances;
        }

        const VkAccelerationStructureGeometryKHR& getTLASGeometry() const {
            return tlasGeometry;
        }
        
    private:
        VkAccelerationStructureGeometryInstancesDataKHR tlasGeometryInstances;
        VkAccelerationStructureGeometryKHR tlasGeometry;
        uint32_t tlasInstanceCount;
};