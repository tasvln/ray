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

enum DescriptorBindingIndices : uint32_t {
    BINDING_UNIFORM_BUFFER = 0,
    BINDING_MATERIAL_BUFFER = 1,
    BINDING_TEXTURE_SAMPLERS = 2,
};

class VulkanGraphicsPipeline{
    public:
        VulkanGraphicsPipeline(
            const VulkanDevice& device, 
            const VulkanSwapChain& swapchain, 
            const VulkanDepthBuffer& depthBuffer,
            const std::vector<VulkanUniformBuffer>& uniformBuffers,
            const VulkanSceneResources& sceneResources,
            bool isWireFrame = false
        ) : device(device), isWireFrame(isWireFrame) {
            createGraphicsPipeline(swapchain, uniformBuffers, sceneResources, depthBuffer);
        }

        ~VulkanGraphicsPipeline() {
            if (pipeline) {
                vkDestroyPipeline(device.getDevice(), pipeline, nullptr);
            }
        }

        const VkPipeline getPipeline() const {
            return pipeline;
        }

        // Descriptor Pool
        const VulkanDescriptorPool& getDescriptorPool() const {
            return *graphicsPool;
        }
        // VulkanDescriptorPool& getDescriptorPool() {
        //     return *graphicsPool;
        // }

        // Descriptor Set Layout
        const VulkanDescriptorSetLayout& getDescriptorSetLayout() const {
            return *graphicsSetLayout;
        }
        // VulkanDescriptorSetLayout& getDescriptorSetLayout() {
        //     return *graphicsSetLayout;
        // }

        // Descriptor Sets
        const VulkanDescriptorSets& getDescriptorSets() const {
            return *graphicsSets;
        }
        // VulkanDescriptorSets& getDescriptorSets() {
        //     return *graphicsSets;
        // }

        // Pipeline Layout
        const VulkanPipelineLayout& getPipelineLayout() const {
            return *graphicsPipelineLayout;
        }
        // VulkanPipelineLayout& getPipelineLayout() {
        //     return *graphicsPipelineLayout;
        // }

        // Render Pass
        const VulkanRenderPass& getRenderPass() const {
            return *graphicsRenderPass;
        }
        // VulkanRenderPass& getRenderPass() {
        //     return *graphicsRenderPass;
        // }

        bool getWireFrameState() const {
            return isWireFrame;
        }

        VkDescriptorSet getDescriptorSet(const size_t index) const
        {
            return graphicsSets->getSet(index);
        }

        
    private:
        const VulkanDevice& device;
        VkPipeline pipeline = VK_NULL_HANDLE;

        std::unique_ptr<VulkanDescriptorPool> graphicsPool;
        std::unique_ptr<VulkanDescriptorSetLayout> graphicsSetLayout;
        std::unique_ptr<VulkanDescriptorSets> graphicsSets;

        std::unique_ptr<VulkanPipelineLayout> graphicsPipelineLayout;
        std::unique_ptr<VulkanRenderPass> graphicsRenderPass;

        bool isWireFrame;

        void createGraphicsPipeline(
            const VulkanSwapChain& swapchain, 
            const std::vector<VulkanUniformBuffer>& uniformBuffers,
            const VulkanSceneResources& sceneResources,
            const VulkanDepthBuffer& depthBuffer
        ) {
            VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
            vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

            VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
            inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            inputAssembly.primitiveRestartEnable = VK_FALSE;

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(swapchain.getSwapChainExtent().width);
            viewport.height = static_cast<float>(swapchain.getSwapChainExtent().height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;

            VkRect2D scissor{};
            scissor.offset = { 0, 0 };
            scissor.extent = swapchain.getSwapChainExtent();

            VkPipelineViewportStateCreateInfo viewportState{};
            viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewportState.viewportCount = 1;
            viewportState.pViewports = &viewport;
            viewportState.scissorCount = 1;
            viewportState.pScissors = &scissor;

            VkPipelineRasterizationStateCreateInfo rasterizer{};
            rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rasterizer.depthClampEnable = VK_FALSE;
            rasterizer.polygonMode = isWireFrame ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
            rasterizer.lineWidth = 1.0f;
            rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
            rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            rasterizer.depthBiasEnable = VK_FALSE;
            rasterizer.depthBiasConstantFactor = 0.0f; // Optional
            rasterizer.depthBiasClamp = 0.0f; // Optional
            rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

            VkPipelineMultisampleStateCreateInfo multisampling{};
            multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multisampling.sampleShadingEnable = VK_FALSE;
            multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

            VkPipelineDepthStencilStateCreateInfo depthStencil = {};
            depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depthStencil.depthTestEnable = VK_TRUE;
            depthStencil.depthWriteEnable = VK_TRUE;
            depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
            depthStencil.depthBoundsTestEnable = VK_FALSE;
            depthStencil.minDepthBounds = 0.0f; // Optional
            depthStencil.maxDepthBounds = 1.0f; // Optional
            depthStencil.stencilTestEnable = VK_FALSE;
            depthStencil.front = {}; // Optional
            depthStencil.back = {}; // Optional

            VkPipelineColorBlendAttachmentState colorBlendAttachment{};
            colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            colorBlendAttachment.blendEnable = VK_FALSE;

            VkPipelineColorBlendStateCreateInfo colorBlending{};
            colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlending.logicOpEnable = VK_FALSE;
            colorBlending.logicOp = VK_LOGIC_OP_COPY;   
            colorBlending.attachmentCount = 1;
            colorBlending.pAttachments = &colorBlendAttachment;

            std::vector<DescriptorBinding> descriptorBindings = {
                {BINDING_UNIFORM_BUFFER, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT}, // Per-frame uniform data (camera, etc.)
                {BINDING_MATERIAL_BUFFER, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}, // Materials buffer
                {BINDING_TEXTURE_SAMPLERS, static_cast<uint32_t>(sceneResources.getTextureSamplers().size()), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT} // Array of texture samplers
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

            graphicsPool = std::make_unique<VulkanDescriptorPool>(device.getDevice(), descriptorBindings, uniformBuffers.size());
            graphicsSetLayout = std::make_unique<VulkanDescriptorSetLayout>(device.getDevice(), descriptorBindings);
            graphicsSets = std::make_unique<VulkanDescriptorSets>(device.getDevice(), *graphicsPool, *graphicsSetLayout, bindingTypes, uniformBuffers.size());

            auto& textureImageViews = sceneResources.getTextureImageViews();
            auto& textureSamplers = sceneResources.getTextureSamplers();

            for (uint32_t i = 0; i != swapchain.getSwapChainImages().size(); i++) {
                // Uniform Buffer
                VkDescriptorBufferInfo uniformBufferInfo = {};
                uniformBufferInfo.buffer = uniformBuffers[i].getBuffer().getBuffer();
                uniformBufferInfo.range = VK_WHOLE_SIZE;

                // Material Buffer
		        VkDescriptorBufferInfo materialBufferInfo = {};
                materialBufferInfo.buffer = sceneResources.getMaterialBuffer().getBuffer();
                uniformBufferInfo.range = VK_WHOLE_SIZE;

                // Texture Buffer
                std::vector<VkDescriptorImageInfo> imageInfos(textureSamplers.size());

                for(size_t j = 0; j != imageInfos.size(); j++) {
                    imageInfos[j].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    imageInfos[j].imageView = textureImageViews[j];
                    imageInfos[j].sampler = textureSamplers[j];
                }

                const std::vector<VkWriteDescriptorSet> descriptorWrites = {
                    graphicsSets->bind(i, 0, uniformBufferInfo),
                    graphicsSets->bind(i, 1, materialBufferInfo),
                    graphicsSets->bind(i, 2, *imageInfos.data(), static_cast<uint32_t>(imageInfos.size())),    
                };

                graphicsSets->updateDescriptors(descriptorWrites);
            }

            graphicsPipelineLayout = std::make_unique<VulkanPipelineLayout>(device.getDevice(), *graphicsSetLayout);
            graphicsRenderPass = std::make_unique<VulkanRenderPass>(device.getDevice(), swapchain.getSwapChainFormat(), depthBuffer, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_LOAD_OP_CLEAR);

            const VulkanShaderModule vShader(device.getDevice(), "shaders/graphics/vert.spv");
            const VulkanShaderModule fShader(device.getDevice(), "shaders/graphics/frag.spv");
            
            VkPipelineShaderStageCreateInfo vShaderStage = vShader.createShaderStage(VK_SHADER_STAGE_VERTEX_BIT);
            VkPipelineShaderStageCreateInfo fShaderStage = vShader.createShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT);
            
            VkPipelineShaderStageCreateInfo shaderStages[] = {
                vShaderStage,
                fShaderStage
            };

            VkGraphicsPipelineCreateInfo pipelineInfo{};
            pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipelineInfo.stageCount = 2;
            pipelineInfo.pStages = shaderStages;
            pipelineInfo.pVertexInputState = &vertexInputInfo;
            pipelineInfo.pInputAssemblyState = &inputAssembly;
            pipelineInfo.pViewportState = &viewportState;
            pipelineInfo.pRasterizationState = &rasterizer;
            pipelineInfo.pMultisampleState = &multisampling;
            pipelineInfo.pColorBlendState = &colorBlending;
            pipelineInfo.layout = graphicsPipelineLayout->getPipelineLayout();
            pipelineInfo.renderPass = graphicsRenderPass->getRenderPass();
            pipelineInfo.subpass = 0;
            pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
            pipelineInfo.basePipelineIndex = -1;

            if (vkCreateGraphicsPipelines(device.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
                throw std::runtime_error("Failed to create graphics pipeline!");
        }
};