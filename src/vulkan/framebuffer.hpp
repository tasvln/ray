#pragma once

#include <vulkan/vulkan.hpp>

#include <iostream>
#include <vector>
#include <array>
#include <stdexcept>

class VulkanFrameBuffer {
    public:
        VulkanFrameBuffer(const VkDevice& device, const VkImageView& imageView, const VkRenderPass& renderPass, const VkExtent2D& swapChainExtent): device(device) {
            createFramebuffers(imageView, swapChainExtent, renderPass);
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

        void createFramebuffers(const VkImageView& imageView, const VkExtent2D& swapChainExtent, const VkRenderPass& renderPass) {
            VkImageView attachments[] = {
                imageView
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = swapChainExtent.width;
            framebufferInfo.height = swapChainExtent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create framebuffer!");
            }
        }

};