#pragma once

#include "engine_interface.hpp"
#include "engine_config.hpp"
#include "core/window.hpp"

#include "vulkan/raster/instance.hpp"
#include "vulkan/raster/surface.hpp"
#include "vulkan/raster/device.hpp"
#include "vulkan/raster/swapchain.hpp"
#include "vulkan/raster/depth_buffer.hpp"
#include "vulkan/raster/uniform_buffer.hpp"
#include "vulkan/raster/graphics_pipeline.hpp"
#include "vulkan/raster/render_pass.hpp"
#include "vulkan/raster/command_pool.hpp"
#include "vulkan/raster/command_buffers.hpp"
#include "vulkan/raster/framebuffer.hpp"

#include "vulkan/raster/semaphore.hpp"
#include "vulkan/raster/fence.hpp"

#include <memory>
#include <vector>

class VulkanRasterEngine: public EngineInterface {
    public:

        VulkanRasterEngine(
            const EngineConfig& config,
            const VulkanSceneResources& resources,
            const Window& window
        ):
            resources(resources),
            window(window)
        {}

        ~VulkanRasterEngine() {
            clearSwapChain();
        }

        // function to call -> without ray
        void createRasterDevice() {
            if (device) 
                throw std::runtime_error("Physical device has already been created");
            
            std::vector<const char*> requiredExtensions = 
            {
                // VK_KHR_swapchain
                VK_KHR_SWAPCHAIN_EXTENSION_NAME
            };

            VkPhysicalDeviceFeatures deviceFeatures {};

            createDevice(
                requiredExtensions,
                deviceFeatures,
                nullptr
            );

            createSwapChain();
        }

        // function to call -> ray
        void createDevice(
            const std::vector<const char*>& requiredExtensions,
            const VkPhysicalDeviceFeatures& deviceFeatures,
            const void* nextDeviceFeatures
        ) {
            if (device) 
                throw std::runtime_error("Physical device has already been created");

            device = std::make_unique<VulkanDevice>(
                instance, 
                surface,
                requiredExtensions,
                deviceFeatures,
                nextDeviceFeatures
            );
            
            commandPool = std::make_unique<VulkanCommandPool>(device->getDevice(), device->getGraphicsFamilyIndex(), true);
        }

        void createSwapChain() {
            while (window->isMinimized()) 
                window->wait();

            swapchain = std::make_unique<VulkanSwapChain>(window, *device, surface, config.presentMode);
            depthBuffer = std::make_unique<VulkanDepthBuffer>(*device, *commandPool, swapchain->getSwapChainExtent());

            for (size_t i = 0; i != swapchain->getSwapChainImages().size(); i++) {
                imageAvailableSemaphores.emplace_back(*device);
                renderFinishedSemaphores.emplace_back(*device);
                inFlightFences.emplace_back(*device, true);
                uniformBuffers.emplace_back(*device);
            }

            graphicsPipeline = std::make_unique<VulkanGraphicsPipeline>(
                *device, 
                *swapchain, 
                *depthBuffer, 
                uniformBuffers, 
                resources, 
                config.enableWireframeMode
            );

            for (const auto& imageView : swapchain->getSwapChainImageViews()) {
                frameBuffers.emplace_back(
                    device->getDevice(), 
                    imageView->getImageView(),
                    depthBuffer->getImageView(),
                    graphicsPipeline->getRenderPass(), 
                    swapchain->getSwapChainExtent()
                );
            }

            commandBuffers = std::make_unique<VulkanCommandBuffers>(
                device->getDevice(),
                *commandPool,
                static_cast<uint32_t>(frameBuffers.size())
            );
        }

        void clearSwapChain() {
            commandBuffers.reset();
            frameBuffers.clear();
            graphicsPipeline.reset();
            uniformBuffers.clear();
            inFlightFences.clear();
            renderFinishedSemaphores.clear();
            imageAvailableSemaphores.clear();
            depthBuffer.reset();
            swapchain.reset();
        }

        void reCreateSwapChain() {
            // wait for the device
            device->wait();

            clearSwapChain();
            createSwapChain(); 
        }

        // in another file
        void updateUniformBuffer() {
            uniformBuffers[currentFrame].setValue(getUBO(swapchain->getSwapChainExtent()));
        }

        // record command buffer but the renderPass portion
        void render(VkCommandBuffer commandBuffer, const size_t currentFrame, const uint32_t imageIndex) {
            const auto clearValues = getClearValues();

            VkRenderPassBeginInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = graphicsPipeline->getRenderPass().getRenderPass();
            renderPassInfo.framebuffer = frameBuffers[imageIndex].getFramebuffer();
            renderPassInfo.renderArea.offset = { 0, 0 };
            renderPassInfo.renderArea.extent = swapchain->getSwapChainExtent();
            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            // const auto& resource = resources;

            VkDescriptorSet descriptorSets[] = { 
                graphicsPipeline->getDescriptorSet(currentFrame)
            };

            VkBuffer vertexBuffers[] = {
                resources.getVertexBuffer().getBuffer()
            };

            const VkBuffer indexBuffer = resources.getIndexBuffer().getBuffer();

            VkDeviceSize offsets[] = {
                0
            };

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->getPipeline());
            vkCmdBindDescriptorSets(
                commandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS, 
                graphicsPipeline->getPipelineLayout().getPipelineLayout(),
                0, 
                1, 
                descriptorSets, 
                0, 
                nullptr
            );

            // this is just the vkCmdDraw but for rendering models
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

            drawModels(commandBuffer);

            vkCmdEndRenderPass(commandBuffer);
        }

        std::array<VkClearValue, 2> getClearValues()
        {
            std::array<VkClearValue, 2> clearValues = {};
            clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
            clearValues[1].depthStencil = { 1.0f, 0 };
            return clearValues;
        }

        void drawModels(VkCommandBuffer commandBuffer)
        {
            uint32_t vertexOffset = 0;
            uint32_t indexOffset = 0;

            for (const auto& model : resources.getModels()) {
                const auto vertexCount = static_cast<uint32_t>(model.getNumOfVertices());
                const auto indexCount = static_cast<uint32_t>(model.getNumOfIndices());

                vkCmdDrawIndexed(commandBuffer, indexCount, 1, indexOffset, vertexOffset, 0);

                vertexOffset += vertexCount;
			    indexOffset += indexCount;
            }
        }

        void drawFrame() {
            constexpr auto noTimeout = std::numeric_limits<uint64_t>::max();

            auto& inFlightFence = inFlightFences[currentFrame];
            const auto imageAvailableSemaphore = imageAvailableSemaphores[currentFrame].getSemaphore();
            const auto renderFinishSemaphore = renderFinishedSemaphores[currentFrame].getSemaphore();

            inFlightFence.wait(noTimeout);

            uint32_t imageIndex;
            if(!getAcquiredNextImage(imageIndex)) return;

            auto commandBuffer = commandBuffers->begin(currentFrame);
            render(commandBuffer, currentFrame, imageIndex);
            commandBuffers->end(currentFrame);

            updateUniformBuffer();

            submitRender(commandBuffer, imageAvailableSemaphore, renderFinishSemaphore);

            if (!presentImage(imageIndex)) return;

            currentFrame = (currentFrame + 1) % inFlightFences.size();
        }
        
        bool presentImage(uint32_t imageIndex) {
            VkSemaphore waitSemaphores[] = {renderFinishedSemaphores[currentFrame].getSemaphore()};

            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = waitSemaphores;
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = &swapchain->getSwapChain();
            presentInfo.pImageIndices = &imageIndex;

            auto result = vkQueuePresentKHR(device->getPresentQueue(), &presentInfo);

            if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
            {
                reCreateSwapChain();
                return false;
            }

            if (result != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to present image: " + std::to_string(result));
            }

            return true;
        }

        void submitRender(VkCommandBuffer commandBuffer, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore)
        {
            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

            VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &waitSemaphore;
            submitInfo.pWaitDstStageMask = waitStages;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &signalSemaphore;

            inFlightFences[currentFrame].reset();

            if (vkQueueSubmit(device->getGraphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame].getFence()) != VK_SUCCESS){
                throw std::runtime_error("Failed to submit draw command to commad buffer");
            }
        }

        std::optional<uint32_t> getAcquiredNextImage(uint32_t& imageIndex)
        {
            constexpr auto noTimeout = std::numeric_limits<uint64_t>::max();

            auto result = vkAcquireNextImageKHR(
                device->getDevice(), 
                swapchain->getSwapChain(), 
                noTimeout, 
                imageAvailableSemaphores[currentFrame].getSemaphore(), 
                nullptr, 
                &imageIndex
            );

            if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || config.enableWireframeMode != graphicsPipeline->getWireFrameState())
            {
                reCreateSwapChain();
                return std::nullopt;
            }

            if (result != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to acquire next image: " + std::to_string(result));
            }

            return imageIndex;
        }

        // this can be defined somewhere else
        // void run() {
        //     if (device) 
        //         throw std::runtime_error("Physical device has not been created");
            
        //     device->wait();
        // }

        // setter
        void setCurrentFrame() {

        }

        const VulkanDevice& getDevice() const {
            return *device;
        }

        VulkanSwapChain& getSwapChain() const {
            return *swapchain;
        }

        VulkanDepthBuffer& getDepthBuffer() const {
            return *depthBuffer;
        }

        VulkanGraphicsPipeline& getGraphicsPipeline() const {
            return *graphicsPipeline;
        }

        VulkanCommandPool& getCommandPool() const {
            return *commandPool;
        }

        VulkanCommandBuffers& getCommandBuffers() const {
            return *commandBuffers;
        }

        const std::vector<VulkanUniformBuffer>& getUniformBuffers() const  {
            return uniformBuffers;
        }

        const std::vector<VulkanFrameBuffer>& getFrameBuffers() const  {
            return frameBuffers;
        }

    private:
        const EngineConfig config;
        const VulkanSceneResources& resources;
        const Window& window;
        const VulkanInstance instance;
        const VulkanSurface surface;

        // just moving things around so it's easier to understand -> the engine is basically done just do the right thing
        // and set the device automatically...

        size_t currentFrame;

        // Raster
        std::unique_ptr<VulkanDevice> device;
        std::unique_ptr<VulkanSwapChain> swapchain;
        std::unique_ptr<VulkanDepthBuffer> depthBuffer;
        std::vector<VulkanUniformBuffer> uniformBuffers;

        std::unique_ptr<VulkanGraphicsPipeline> graphicsPipeline;

        std::unique_ptr<VulkanCommandPool> commandPool;
        std::unique_ptr<VulkanCommandBuffers> commandBuffers;

        std::vector<VulkanFrameBuffer> frameBuffers;

        // Semaphore & Fences
        std::vector<VulkanSemaphore> imageAvailableSemaphores;
        std::vector<VulkanSemaphore> renderFinishedSemaphores;
        std::vector<VulkanFence> inFlightFences;
};