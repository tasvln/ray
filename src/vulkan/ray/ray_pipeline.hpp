#pragma once

#include "vulkan/raster/swapchain.hpp"
#include "vulkan/raster/pipeline_layout.hpp"
#include "vulkan/raster/descriptor_sets.hpp"
#include "vulkan/raster/descriptor_pool.hpp"
#include "vulkan/raster/descriptorset_layout.hpp"
#include "vulkan/raster/render_pass.hpp"
#include "vulkan/raster/shader_module.hpp"
#include "vulkan/raster/image_view.hpp"
#include "helpers/vertex.hpp"
#include "vulkan/raster/uniform_buffer.hpp"
#include "helpers/scene_resources.hpp"
#include "vulkan/raster/depth_buffer.hpp"
#include "tlas.hpp"
#include "blas.hpp"

#include <iostream>
#include <vector>
#include <stdexcept>
#include <map>
#include <utility>

enum RayTracingBindingIndices : uint32_t {
    BINDING_ACCELERATION_STRUCTURE = 0,
    BINDING_ACCUMULATION_IMAGE     = 1,
    BINDING_OUTPUT_IMAGE           = 2,
    BINDING_UNIFORM_BUFFER         = 3,
    BINDING_VERTEX_BUFFER          = 4,
    BINDING_INDEX_BUFFER           = 5,
    BINDING_MATERIAL_BUFFER        = 6,
    BINDING_OFFSET_BUFFER          = 7,
    BINDING_TEXTURE_SAMPLERS       = 8,
    BINDING_PROCEDURAL_BUFFER      = 9
};


class VulkanRayPipeline {
    public:
        VulkanRayPipeline(
            const VulkanDevice& device,
            const VulkanSwapChain& swapchain, 
            const std::vector<VulkanUniformBuffer>& uniformBuffers,
            const VulkanSceneResources& resources,
            const VulkanDepthBuffer& depthBuffer,
            const VulkanRayTLAS& tlas,
            const VulkanImageView& accumulationImageView,
            const VulkanImageView& outputImageView,
            const VulkanRayDispatchTable& dispatch
        ) : device(device) {
            createRayPipeline(
                swapchain, 
                uniformBuffers, 
                resources, 
                depthBuffer, 
                tlas, 
                accumulationImageView,
                outputImageView,
                dispatch
            );
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

        uint32_t getGenShaderIndex() const { 
            return rayGenIndex; 
        }

		uint32_t getMissShaderIndex() const { 
            return missIndex; 
        }

		uint32_t getTriangleHitGroupIndex() const { 
            return triangleHitGroupIndex; 
        }

		uint32_t getProceduralHitGroupIndex() const { 
            return proceduralHitGroupIndex; 
        }

        VkDescriptorSet getDescriptorSet(const size_t index) const
        {
            return raySets->getSet(index);
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
            const VulkanSceneResources& resources,
            const VulkanDepthBuffer& depthBuffer,
            const VulkanRayTLAS& tlas,
            const VulkanImageView& accumulationImageView,
            const VulkanImageView& outputImageView,
            const VulkanRayDispatchTable& dispatch
        ) {
            const std::vector<DescriptorBinding> descriptorBindings =
            {
                {BINDING_ACCELERATION_STRUCTURE, 1, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR},

                {BINDING_ACCUMULATION_IMAGE, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
                {BINDING_OUTPUT_IMAGE, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR},

                {BINDING_UNIFORM_BUFFER, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR},

                {BINDING_VERTEX_BUFFER, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
                {BINDING_INDEX_BUFFER, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
                {BINDING_MATERIAL_BUFFER, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
                {BINDING_OFFSET_BUFFER, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},

                {BINDING_TEXTURE_SAMPLERS, static_cast<uint32_t>(resources.getTextureSamplers().size()), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},

                {BINDING_PROCEDURAL_BUFFER, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR}
            };

            // setup
            std::map<uint32_t, VkDescriptorType> bindingTypes;

            for (const auto& binding : descriptorBindings)
            {
                if (!bindingTypes.insert(std::make_pair(binding.binding, binding.descriptorType)).second)
                {
                    throw std::invalid_argument("binding collision");
                }
            }

            rayPool = std::make_unique<VulkanDescriptorPool>(device.getDevice(), descriptorBindings, uniformBuffers.size());
            raySetLayout = std::make_unique<VulkanDescriptorSetLayout>(device.getDevice(), descriptorBindings);
            raySets = std::make_unique<VulkanDescriptorSets>(device.getDevice(), *rayPool, *raySetLayout, bindingTypes, uniformBuffers.size());

            auto& textureImageViews = resources.getTextureImageViews();
            auto& textureSamplers = resources.getTextureSamplers();

            for (uint32_t i = 0; i != swapchain.getSwapChainImages().size(); i++) {
                // TLAS ->
                const auto accelerationStructure = tlas.getStructure();

                VkWriteDescriptorSetAccelerationStructureKHR structureInfo{};
                structureInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
                structureInfo.pNext = nullptr;
                structureInfo.accelerationStructureCount = 1;
                structureInfo.pAccelerationStructures = &accelerationStructure;

                // Accumulation image
                VkDescriptorImageInfo accumulationImageInfo = {};
                accumulationImageInfo.imageView = accumulationImageView.getImageView();
                accumulationImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

                // Output image
                VkDescriptorImageInfo outputImageInfo = {};
                outputImageInfo.imageView = outputImageView.getImageView();
                outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

                // Uniform buffer
                VkDescriptorBufferInfo uniformBufferInfo = {};
                uniformBufferInfo.buffer = uniformBuffers[i].getBuffer().getBuffer();
                uniformBufferInfo.range = VK_WHOLE_SIZE;

                // Vertex buffer
                VkDescriptorBufferInfo vertexBufferInfo = {};
                vertexBufferInfo.buffer = resources.getVertexBuffer().getBuffer();
                vertexBufferInfo.range = VK_WHOLE_SIZE;

                // Index buffer
                VkDescriptorBufferInfo indexBufferInfo = {};
                indexBufferInfo.buffer = resources.getIndexBuffer().getBuffer();
                indexBufferInfo.range = VK_WHOLE_SIZE;

                // Material buffer
                VkDescriptorBufferInfo materialBufferInfo = {};
                materialBufferInfo.buffer = resources.getMaterialBuffer().getBuffer();
                materialBufferInfo.range = VK_WHOLE_SIZE;

                // Offsets buffer
                VkDescriptorBufferInfo offsetsBufferInfo = {};
                offsetsBufferInfo.buffer = resources.getOffsetBuffer().getBuffer();
                offsetsBufferInfo.range = VK_WHOLE_SIZE;
                
                // Texture Buffer
                std::vector<VkDescriptorImageInfo> imageInfos(textureSamplers.size());

                for(size_t j = 0; j != imageInfos.size(); j++) {
                    imageInfos[j].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    imageInfos[j].imageView = textureImageViews[j];
                    imageInfos[j].sampler = textureSamplers[j];
                }

                std::vector<VkWriteDescriptorSet> descriptorWrites = {
                    raySets->bind(i, 0, structureInfo),
                    raySets->bind(i, 1, accumulationImageInfo),
                    raySets->bind(i, 2, outputImageInfo),
                    raySets->bind(i, 3, uniformBufferInfo),
                    raySets->bind(i, 4, vertexBufferInfo),
                    raySets->bind(i, 5, indexBufferInfo),
                    raySets->bind(i, 6, materialBufferInfo),
                    raySets->bind(i, 7, offsetsBufferInfo),
                    raySets->bind(i, 8, *imageInfos.data(), static_cast<uint32_t>(imageInfos.size()))
                };

                // optional + copied from reference github repo
                VkDescriptorBufferInfo proceduralBufferInfo = {};

                if (resources.isProcedurals()) {
                    proceduralBufferInfo.buffer = resources.getProceduralBuffer().getBuffer();
                    proceduralBufferInfo.range = VK_WHOLE_SIZE;

                    descriptorWrites.push_back(raySets->bind(i, 9, proceduralBufferInfo));
                }

                raySets->updateDescriptors(descriptorWrites);
            }

            rayPipelineLayout = std::make_unique<VulkanPipelineLayout>(device.getDevice(), *raySetLayout);

            
            const VulkanShaderModule rayGenShader(device.getDevice(), "shaders/ray/rgen.spv");
            const VulkanShaderModule rayMissShader(device.getDevice(), "shaders/ray/rmiss.spv");
            const VulkanShaderModule rayClosestHitShader(device.getDevice(), "shaders/ray/rchit.spv");
            
            // optional?? -> comment out and see
            const VulkanShaderModule rayProceduralClosestHitShader(device.getDevice(), "shaders/ray/rpchit.spv");
            const VulkanShaderModule rayProceduralIntersectionShader(device.getDevice(), "shaders/ray/rpint.spv");

            VkPipelineShaderStageCreateInfo rayGenShaderStage = rayGenShader.createShaderStage(VK_SHADER_STAGE_RAYGEN_BIT_KHR);
            VkPipelineShaderStageCreateInfo rayMissShaderStage = rayMissShader.createShaderStage(VK_SHADER_STAGE_MISS_BIT_KHR);
            VkPipelineShaderStageCreateInfo rayClosestHitShaderStage = rayClosestHitShader.createShaderStage(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);

            VkPipelineShaderStageCreateInfo rayProceduralClosestHitShaderStage = rayProceduralClosestHitShader.createShaderStage(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
            VkPipelineShaderStageCreateInfo rayProceduralIntersectionStage = rayProceduralIntersectionShader.createShaderStage(VK_SHADER_STAGE_INTERSECTION_BIT_KHR);

            std::vector<VkPipelineShaderStageCreateInfo> shaderStages =
            {
                rayGenShaderStage,
                rayMissShaderStage,
                rayClosestHitShaderStage,

                rayProceduralClosestHitShaderStage,
                rayProceduralIntersectionStage
            };

            // Shader Groups -> TODO -> refactor
	        VkRayTracingShaderGroupCreateInfoKHR rayGenGroupInfo = {};
            rayGenGroupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            rayGenGroupInfo.pNext = nullptr;
            rayGenGroupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
            rayGenGroupInfo.generalShader = 0;
            rayGenGroupInfo.closestHitShader = VK_SHADER_UNUSED_KHR;
            rayGenGroupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
            rayGenGroupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;
            rayGenIndex = 0;

            VkRayTracingShaderGroupCreateInfoKHR missGroupInfo = {};
            missGroupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            missGroupInfo.pNext = nullptr;
            missGroupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
            missGroupInfo.generalShader = 1;
            missGroupInfo.closestHitShader = VK_SHADER_UNUSED_KHR;
            missGroupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
            missGroupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;
            missIndex = 1;

            VkRayTracingShaderGroupCreateInfoKHR triangleHitGroupInfo = {};
            triangleHitGroupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            triangleHitGroupInfo.pNext = nullptr;
            triangleHitGroupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
            triangleHitGroupInfo.generalShader = VK_SHADER_UNUSED_KHR;
            triangleHitGroupInfo.closestHitShader = 2;
            triangleHitGroupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
            triangleHitGroupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;
            triangleHitGroupIndex = 2;

            VkRayTracingShaderGroupCreateInfoKHR proceduralHitGroupInfo = {};
            proceduralHitGroupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            proceduralHitGroupInfo.pNext = nullptr;
            proceduralHitGroupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
            proceduralHitGroupInfo.generalShader = VK_SHADER_UNUSED_KHR;
            proceduralHitGroupInfo.closestHitShader = 3;
            proceduralHitGroupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
            proceduralHitGroupInfo.intersectionShader = 4;
            proceduralHitGroupIndex = 3;

            std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups =
            {
                rayGenGroupInfo, 
                missGroupInfo, 
                triangleHitGroupInfo, 
                proceduralHitGroupInfo,
            };

            VkRayTracingPipelineCreateInfoKHR pipelineInfo = {};
            pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
            pipelineInfo.pNext = nullptr;
            pipelineInfo.flags = 0;
            pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
            pipelineInfo.pStages = shaderStages.data();
            pipelineInfo.groupCount = static_cast<uint32_t>(groups.size());
            pipelineInfo.pGroups = groups.data();
            pipelineInfo.maxPipelineRayRecursionDepth = 1;
            pipelineInfo.layout = rayPipelineLayout->getPipelineLayout();
            pipelineInfo.basePipelineHandle = nullptr;
            pipelineInfo.basePipelineIndex = 0;


            if (dispatch.vkCreateRayTracingPipelinesKHR(device.getDevice(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
                throw std::runtime_error("Failed to create graphics pipeline!");
    }
};