#pragma once

#include <vulkan/vulkan.hpp>

#include <iostream>
#include <vector>
#include <array>
#include <stdexcept>

#include "depth_buffer.hpp"

// TODO: add depth attachments

class VulkanRenderPass {
    public:
        VulkanRenderPass(
            const VkDevice& device, 
            const VkFormat& swapChainImageFormat,
            const VulkanDepthBuffer& depthBuffer,
            const VkAttachmentLoadOp colorBufferLoadOp,
	        const VkAttachmentLoadOp depthBufferLoadOp
        ): 
            device(device)
        {
            createRenderPass(swapChainImageFormat, depthBuffer, colorBufferLoadOp, depthBufferLoadOp);
        }

        ~VulkanRenderPass() {
            if (renderPass != nullptr) {
                vkDestroyRenderPass(device, renderPass, nullptr);
                renderPass = VK_NULL_HANDLE;
            }
        }

        const VkRenderPass& getRenderPass() const {
            return renderPass;
        }

    private:
        VkDevice device = VK_NULL_HANDLE;
        VkRenderPass renderPass = VK_NULL_HANDLE;

        void createRenderPass(const VkFormat& swapChainImageFormat, const VulkanDepthBuffer& depthBuffer, const VkAttachmentLoadOp colorBufferLoadOp, const VkAttachmentLoadOp depthBufferLoadOp) {
            // Color attachment
            VkAttachmentDescription colorAttachment{};
            colorAttachment.format = swapChainImageFormat;
            colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

            colorAttachment.loadOp = colorBufferLoadOp;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

            colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

            colorAttachment.initialLayout = colorBufferLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

            // Depth attachment
            VkAttachmentDescription depthAttachment = {};
            depthAttachment.format = depthBuffer.getFormat();
            depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            depthAttachment.loadOp = depthBufferLoadOp;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.initialLayout = depthBufferLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            // Attachment reference
            VkAttachmentReference colorAttachmentRef{};
            colorAttachmentRef.attachment = 0;
            colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            // Subpass
            VkSubpassDescription subpass{};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &colorAttachmentRef;
            // subpass.pDepthStencilAttachment = &depthAttachmentRef;

            // Subpass dependency (synchronization)
            VkSubpassDependency dependency{};
            dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            dependency.dstSubpass = 0;

            dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.srcAccessMask = 0;

            dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            std::array<VkAttachmentDescription, 2> attachments =
            { 
                colorAttachment,
                depthAttachment 
            };

            // Create Render Pass
            VkRenderPassCreateInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            renderPassInfo.pAttachments = attachments.data();
            renderPassInfo.subpassCount = 1;
            renderPassInfo.pSubpasses = &subpass;
            renderPassInfo.dependencyCount = 1;
            renderPassInfo.pDependencies = &dependency;

            if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create render pass");
            }
        }
};