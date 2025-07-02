#pragma once

#include <glm/glm.hpp>

#include "acceleration_structure.hpp"
#include "blas_geometry.hpp"

class VulkanRayBLAS : public VulkanRayAccelerationStructure {
    public:
        VulkanRayBLAS(
            const VulkanDevice& device,
            const VulkanRayDispatchTable& dispatch,
            const VulkanRayDeviceProperties& rayDeviceProperties,
            const VulkanRayBLASGeometry& blasGeometries
        ):  VulkanRayAccelerationStructure(device, dispatch, rayDeviceProperties),
            blasGeometries(blasGeometries)
        {
            createGeometry();
        }
        
        VulkanRayBLAS(VulkanRayBLAS&& src) :
            VulkanRayAccelerationStructure(std::move(src)),
            blasGeometries(std::move(src.blasGeometries)) 
        {}

        ~VulkanRayBLAS() = default;

        void createGeometry() {
            buildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
            buildGeometryInfo.flags = flags;
            buildGeometryInfo.geometryCount = static_cast<uint32_t>(blasGeometries.getGeometries().size());
            buildGeometryInfo.pGeometries = blasGeometries.getGeometries().data();
            buildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
            buildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            buildGeometryInfo.srcAccelerationStructure = nullptr;

            std::vector<uint32_t> primitiveCounts(blasGeometries.getBuildRangeInfos().size());

            for (const auto& range : blasGeometries.getBuildRangeInfos()) {
                primitiveCounts.push_back(range.primitiveCount);
            }

            buildSizeInfo = getBuildSizes(primitiveCounts.data());
        }

        
        void generateBLAS(
            VkCommandBuffer commandBuffer,
            VulkanBuffer& buffer,
            const VkDeviceSize offset,
            VulkanBuffer& scratchBuffer,
            const VkDeviceSize scratchOffset
        ) {
            createStructure(buffer, offset);

            const VkAccelerationStructureBuildRangeInfoKHR* structureBuildRanges = blasGeometries.getBuildRangeInfos().data();

            buildGeometryInfo.dstAccelerationStructure = getStructure();
            buildGeometryInfo.scratchData.deviceAddress = scratchBuffer.getDeviceAddress() + scratchOffset;

            dispatch.vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildGeometryInfo, &structureBuildRanges);
        }
        
    private:
        VulkanRayBLASGeometry blasGeometries;
};