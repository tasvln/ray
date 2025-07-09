#pragma once

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

#include "vulkan/ray/blas.hpp"
#include "vulkan/ray/tlas.hpp"

#include <memory>
#include <vector>

class Engine {
    public:

        Engine(
            const EngineConfig& config, 
            const VkPresentModeKHR presentMode
        ):
            presentMode(presentMode)
        {
            const auto validationLayers = config.enableValidationLayers ? 
            std::vector<const char*> {
                "VK_LAYER_KHRONOS_validation"
            } : std::vector<const char*>();

            // create window
            window = std::make_unique<Window>(config);
            // create instance
            instance = std::make_unique<VulkanInstance>(validationLayers);
            // create surface
            surface = std::make_unique<VulkanSurface>(*instance, *window);
        }

        ~Engine() {

        }

        void createDevice() {
            if (device) 
                throw std::runtime_error("Physical device has already been created");
            
            VkPhysicalDeviceFeatures deviceFeatures {};

            device = std::make_unique<VulkanDevice>(*instance, *surface);
            commandPool = std::make_unique<VulkanCommandPool>(device->getDevice(), device->getGraphicsFamilyIndex(), true);
        }

        void createSwapChain(const VulkanSceneResources& resources) {
            while (window->isMinimized()) 
                window->wait();

            swapchain = std::make_unique<VulkanSwapChain>(*window, *device, *surface, presentMode);
            depthBuffer = std::make_unique<VulkanDepthBuffer>(*device, *commandPool, swapchain->getSwapChainExtent());

            for (size_t i = 0; i != swapchain->getSwapChainImages().size(); i++) {
                imageAvailableSemaphore.emplace_back(*device);
                renderFinishedSemaphore.emplace_back(*device);
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

        void resetSwapChain() {
            commandBuffers.reset();
            frameBuffers.clear();
            graphicsPipeline.reset();
            uniformBuffers.clear();
            inFlightFences.clear();
            renderFinishedSemaphore.clear();
            imageAvailableSemaphore.clear();
            depthBuffer.reset();
            swapchain.reset();
        }

        void reCreateSwapChain(const VulkanSceneResources& resources) {
            // wait for the device
            device->wait();

            resetSwapChain();
            createSwapChain(resources);
        }

        void updateUniformBuffer() {
            // uniformBuffers[currentFrame].setValue(getUBO(swapchain->getSwapChainExtent()));
        }

    private:
        EngineConfig config;
        const VkPresentModeKHR presentMode;

        size_t currentFrame;

        // Raster
        std::unique_ptr<Window> window;
        std::unique_ptr<VulkanInstance> instance;
        std::unique_ptr<VulkanSurface> surface;
        std::unique_ptr<VulkanDevice> device;
        std::unique_ptr<VulkanSwapChain> swapchain;
        std::unique_ptr<VulkanDepthBuffer> depthBuffer;
        std::vector<VulkanUniformBuffer> uniformBuffers;

        std::unique_ptr<VulkanGraphicsPipeline> graphicsPipeline;
        // std::vector<VulkanRenderPass> renderPass;

        std::unique_ptr<VulkanCommandPool> commandPool;
        std::unique_ptr<VulkanCommandBuffers> commandBuffers;

        std::vector<VulkanFrameBuffer> frameBuffers;

        // Semaphore & Fences
        std::vector<VulkanSemaphore> imageAvailableSemaphore;
        std::vector<VulkanSemaphore> renderFinishedSemaphore;
        std::vector<VulkanFence> inFlightFences;

        // Ray
        VulkanRayTLAS tlas;
        VulkanRayBLAS blas;
};