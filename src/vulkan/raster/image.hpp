#pragma once

#include "device.hpp"
#include "device_memory.hpp"
#include "command_buffers.hpp"
#include "command_pool.hpp"
#include "buffer.hpp"
#include "depth_buffer.hpp"

class VulkanImage {
    public:
        VulkanImage(
            const VulkanDevice& device,
            VkExtent2D extent, 
            VkFormat format,
            VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL,
            VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        ):  device(device),
            extent(extent),
            format(format),
            layout(initialLayout)   
        {
            VkImageCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            info.imageType = VK_IMAGE_TYPE_2D;
            info.extent = { extent.width, extent.height, 1 };
            info.mipLevels = 1;
            info.arrayLayers = 1;
            info.format = format;
            info.tiling = tiling;
            info.initialLayout = initialLayout;
            info.usage = usage;
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            info.samples = VK_SAMPLE_COUNT_1_BIT;

            if (vkCreateImage(device.getDevice(), &info, nullptr, &image) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create image -> VulkanImage");
            }
        }

        VulkanImage(const VulkanImage&) = delete;
        VulkanImage& operator=(const VulkanImage&) = delete;

        VulkanImage& operator=(VulkanImage&&) = delete;
        VulkanImage(VulkanImage&& other) noexcept
            : device(std::move(other.device))
        {
            moveFrom(std::move(other));
        }

        ~VulkanImage() {
            if (image) {
                vkDestroyImage(device.getDevice(), image, nullptr);
                image = VK_NULL_HANDLE;
            }
        }

        void transitionLayout(VulkanCommandPool& commandPool, VkImageLayout newLayout) {
            VulkanCommandBuffers commandBuffers(device.getDevice(), commandPool, 1);

			VkCommandBufferBeginInfo beginInfo = {};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

            vkBeginCommandBuffer(commandBuffers.getCommandBuffers()[0], &beginInfo);

            // main -> start
            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = layout;
            barrier.newLayout = newLayout;
            barrier.image = image;
            barrier.subresourceRange = {
                .aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                            ? VK_IMAGE_ASPECT_DEPTH_BIT | (VulkanDepthBuffer.hasStencilComponent(format) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0)
                            : VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            };
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

            VkPipelineStageFlags srcStage = 0;
            VkPipelineStageFlags dstStage = 0;

            // TO-DO -> move this to a different function with more layout options
            if (layout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            } else if (layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            } else if (layout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            } else {
                throw std::runtime_error("Unsupported image layout transition");
            }

            vkCmdPipelineBarrier(commandBuffers.getCommandBuffers()[0], srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

            // main -> end

            vkEndCommandBuffer(commandBuffers.getCommandBuffers()[0]);

			VkSubmitInfo submitInfo = {};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffers.getCommandBuffers()[0];

            vkQueueSubmit(device.getGraphicsQueue(), 1, &submitInfo, nullptr);
			vkQueueWaitIdle(device.getGraphicsQueue());
        }

        void copyFrom(VulkanCommandPool& commandPool, VkImageLayout newLayout, VkQueue graphicsQueue) {
            VulkanCommandBuffers commandBuffers(device.getDevice(), commandPool, 1);

			VkCommandBufferBeginInfo beginInfo = {};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

            vkBeginCommandBuffer(commandBuffers.getCommandBuffers()[0], &beginInfo);

            // main -> start
            VkBufferImageCopy copyRegion{};
            copyRegion.bufferOffset = 0;
            copyRegion.bufferRowLength = 0;
            copyRegion.bufferImageHeight = 0;
            
            copyRegion.imageSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1
            };

            copyRegion.imageOffset = { 0, 0, 0 };
            copyRegion.imageExtent = { extent.width, extent.height, 1 };
            // main -> end

            vkEndCommandBuffer(commandBuffers.getCommandBuffers()[0]);

			VkSubmitInfo submitInfo = {};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffers.getCommandBuffers()[0];

            vkQueueSubmit(graphicsQueue, 1, &submitInfo, nullptr);
			vkQueueWaitIdle(graphicsQueue);
        }

        VulkanDeviceMemory allocateMemory(VkMemoryPropertyFlags properties) {
            const auto reqs = GetMemoryRequirements();
            VulkanDeviceMemory memory(device, reqs.memoryTypeBits, 0, properties, reqs.size);

            if (vkBindImageMemory(device.getDevice(), image, memory.getMemory(), 0) != VK_SUCCESS) {
                throw std::runtime_error("Failed to bind image memory -> VulkanImage");
            }

            return memory;
        }

        VkMemoryRequirements GetMemoryRequirements() const {
            VkMemoryRequirements reqs;
            
            vkGetImageMemoryRequirements(device.getDevice(), image, &reqs);

            return reqs;
        }

        const VkImage getImage() const {
            return image;
        }

        const VkExtent2D getExtent() const {
            return extent;
        }

        const VkFormat getFormat() const {
            return format;
        }

        const VkImageLayout getLayout() const {
            return layout;
        }

    private:
        VulkanDevice device;
        VkExtent2D extent;
        VkFormat format;
        VkImageLayout layout;

        VkImage image;

        void moveFrom(VulkanImage&& other) noexcept {
            extent = other.extent;
            format = other.format;
            layout = other.layout;
            image = other.image;
            other.image = VK_NULL_HANDLE;
        }
};