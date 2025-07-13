#pragma once

#include "vulkan/raster/buffer.hpp"
#include "vulkan/raster/device_memory.hpp"
#include <vector>

namespace utils
{
    template <class T>
	VkAccelerationStructureBuildSizesInfoKHR getTotalRequirements(const std::vector<T>& accelerationStructures)
	{
		VkAccelerationStructureBuildSizesInfoKHR total {};

		for (const auto& structure : accelerationStructures)
		{
			total.accelerationStructureSize += structure.getBuildSizeInfo().accelerationStructureSize;
			total.buildScratchSize += structure.getBuildSizeInfo().buildScratchSize;
			total.updateScratchSize += structure.getBuildSizeInfo().updateScratchSize;
		}

		return total;
	}

    struct StructureData {
        std::unique_ptr<VulkanBuffer> buffer;
        std::unique_ptr<VulkanDeviceMemory> deviceMemory;
    };
} // namespace utils
