#pragma once

#include "swapchain.hpp"
#include "pipeline_layout.hpp"
#include "descriptor_sets.hpp"
#include "descriptor_pool.hpp"
#include "descriptorset_layout.hpp"
#include "render_pass.hpp"
#include "shader_module.hpp"
#include "helpers/vertex.hpp"
#include "helpers/uniform_buffer.hpp"
#include "helpers/scene_resources.hpp"
#include "depth_buffer.hpp"

#include <iostream>
#include <vector>
#include <stdexcept>
#include <map>
#include <utility>

class VulkanRayPipeline {
    public:
        VulkanRayPipeline(
            const VulkanDevice& device
        ) : device(device) {

        }

        ~VulkanRayPipeline() {
            if (pipeline) {
                vkDestroyPipeline(device.getDevice(), pipeline, nullptr);
            }
        }

        const VkPipeline getPipeline() const {
            return pipeline;
        }

        const VulkanPipelineLayout& getPipelineLayout() const {
            return *rayPipelineLayout;
        }

        const VulkanDescriptorPool& getDescriptorPool() const {
            return *rayPool;
        }

        const VulkanDescriptorSetLayout& getDescriptorSetLayout() const {
            return *raySetLayout;
        }

        const VulkanDescriptorSets& getDescriptorSets() const {
            return *raySets;
        }

        uint32_t getRayGenShaderIndex() const { 
            return rayGenIndex; 
        }

		uint32_t getMissShaderIndex() const { 
            return missIndex; 
        }

		uint32_t getTriangleHitGroupIndex() const { 
            return triangleHitGroupIndex; 
        }

		uint32_t getroceduralHitGroupIndex() const { 
            return proceduralHitGroupIndex; 
        }

    private:
        VulkanDevice device;
        VkPipeline pipeline = VK_NULL_HANDLE;

        std::unique_ptr<VulkanPipelineLayout> rayPipelineLayout;

        std::unique_ptr<VulkanDescriptorPool> rayPool;
        std::unique_ptr<VulkanDescriptorSetLayout> raySetLayout;
        std::unique_ptr<VulkanDescriptorSets> raySets;

		uint32_t rayGenIndex;
		uint32_t missIndex;
		uint32_t triangleHitGroupIndex;
		uint32_t proceduralHitGroupIndex;

        void createRayPipeline(
            const VulkanSwapChain& swapchain, 
            const std::vector<VulkanUniformBuffer>& uniformBuffers,
            const VulkanSceneResources& sceneResources,
            const VulkanDepthBuffer& depthBuffer
        ) {
            
        }
};