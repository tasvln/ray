#pragma once

#include "vulkan/raster/buffer.hpp"
#include "vulkan/raster/device_memory.hpp"
#include "vulkan/raster/image.hpp"
#include "vulkan/raster/image_view.hpp"
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

    struct ImageData {
		std::unique_ptr<VulkanImage> image;
		std::unique_ptr<VulkanDeviceMemory> memory;
		std::unique_ptr<VulkanImageView> imageView;

		void clear() {
			image.reset();
			memory.reset();
			imageView.reset();
		}
    };
} // namespace utils
