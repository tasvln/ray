#pragma once

#include <vulkan/vulkan.hpp>

#include <iostream>
#include <vector>
#include <array>
#include <stdexcept>

#include "depth_buffer.hpp"
#include "render_pass.hpp"

class VulkanFrameBuffer {
    public:
        VulkanFrameBuffer(const VkDevice& device, const VkImageView& colorImageView, const VkImageView& depthImageView, const VulkanRenderPass& renderPass, const VkExtent2D& swapChainExtent): device(device) {
            createFramebuffers(colorImageView, depthImageView, swapChainExtent, renderPass);
        }

        ~VulkanFrameBuffer() {
            if (framebuffer != nullptr) {
                vkDestroyFramebuffer(device, framebuffer, nullptr);
		        framebuffer = nullptr;
            }
        }

        const VkFramebuffer& getFramebuffer() const {
            return framebuffer;
        }

    private:
        VkDevice device = VK_NULL_HANDLE;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;

        void createFramebuffers(const VkImageView& colorImageView, const VkImageView& depthImageView, const VkExtent2D& swapChainExtent, const VulkanRenderPass& renderPass) {
            VkImageView attachments[] = {
                colorImageView,
                depthImageView
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass.getRenderPass();
            framebufferInfo.attachmentCount = 2;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = swapChainExtent.width;
            framebufferInfo.height = swapChainExtent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create framebuffer!");
            }
        }

};