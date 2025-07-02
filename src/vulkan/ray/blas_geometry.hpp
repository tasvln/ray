#pragma once

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <vector>

#include "helpers/scene_resources.hpp"

class VulkanRayBLASGeometry {
    public:
        VulkanRayBLASGeometry() = default;
        ~VulkanRayBLASGeometry() = default;

        void addTriangles(
            const VulkanSceneResources& resources,
            uint32_t vertexOffset,
            uint32_t vertexCount,
            uint32_t indexOffset,
            uint32_t indexCount,
            bool isOpaque
        ) {
            const VkDeviceAddress vertexAddress = resources.getVertexBuffer().getDeviceAddress();
            const VkDeviceAddress indexAddress = resources.getIndexBuffer().getDeviceAddress();
            constexpr VkFormat vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;

            VkAccelerationStructureGeometryKHR geometry = createGeometry(
                VK_GEOMETRY_TYPE_TRIANGLES_KHR,
                isOpaque ? VK_GEOMETRY_OPAQUE_BIT_KHR : 0
            );

            auto& triangles = geometry.geometry.triangles;
            triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
            triangles.pNext = nullptr;
            triangles.vertexFormat = vertexFormat;
            triangles.vertexData.deviceAddress = vertexAddress;
            triangles.vertexStride = sizeof(VulkanVertex);
            triangles.maxVertex = vertexCount;
            triangles.indexType = VK_INDEX_TYPE_UINT32;
            triangles.transformData = {};
            triangles.indexData.deviceAddress = indexAddress;
            triangles.transformData.deviceAddress = 0;

            VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
            buildRangeInfo.firstVertex = vertexOffset / sizeof(VulkanVertex);
            buildRangeInfo.primitiveOffset = indexOffset;
            buildRangeInfo.primitiveCount = indexCount / 3;
            buildRangeInfo.transformOffset = 0;

            geometries.emplace_back(geometry);
            buildRangeInfos.emplace_back(buildRangeInfo);
        }

        void addAaBb(
            const VulkanSceneResources& resources,
            uint32_t aabbOffset,
            uint32_t aabbCount,
            bool isOpaque
        ) {
            const VkDeviceAddress aabbAddress = resources.getAaBbBuffer().getDeviceAddress();

            VkAccelerationStructureGeometryKHR geometry = createGeometry(
                VK_GEOMETRY_TYPE_AABBS_KHR,
                isOpaque ? VK_GEOMETRY_OPAQUE_BIT_KHR : 0
            );

            auto& aabbs = geometry.geometry.aabbs;
            aabbs.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
            aabbs.pNext = nullptr;
            aabbs.data.deviceAddress = aabbAddress;
            aabbs.stride = sizeof(VkAabbPositionsKHR);

            VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
            buildRangeInfo.firstVertex = 0;
            buildRangeInfo.primitiveOffset = aabbOffset;
            buildRangeInfo.primitiveCount = aabbCount;
            buildRangeInfo.transformOffset = 0;

            geometries.emplace_back(geometry);
            buildRangeInfos.emplace_back(buildRangeInfo);
        }

        VkAccelerationStructureGeometryKHR createGeometry(VkGeometryTypeKHR type, VkGeometryFlagsKHR flags) {
            VkAccelerationStructureGeometryKHR geometry{};
            geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
            geometry.geometryType = type;
            geometry.flags = flags;
            geometry.pNext = nullptr;

            return geometry;
        }

        const std::vector<VkAccelerationStructureGeometryKHR>& getGeometries() const {
            return geometries;
        }

        const std::vector<VkAccelerationStructureBuildRangeInfoKHR>& getBuildRangeInfos() const {
            return buildRangeInfos;
        }

    private:
        // The geometry to build, addresses of vertices and indices.
		std::vector<VkAccelerationStructureGeometryKHR> geometries;

		// the number of elements to build and offsets
		std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRangeInfos;
};