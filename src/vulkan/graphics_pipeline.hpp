#pragma once

#include "swapchain.hpp"
#include "pipeline_layout.hpp"
#include "descriptor_sets.hpp"
#include "descriptor_pool.hpp"
#include "descriptorset_layout.hpp"
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
            const VkDevice& device, 
            const VulkanSwapChain& swapchain, 
            const VulkanDepthBuffer& depthBuffer,
            const bool isWireFrame = false,
            const std::vector<VulkanUniformBuffer>& uniformBuffers
        ): device(device), isWireFrame(isWireFrame) {

        }

        ~VulkanGraphicsPipeline() {

        }
        
    private:
        VkDevice device = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;

        const bool isWireFrame;

        void createGraphicsPipeline(
            const VulkanSwapChain& swapchain, 
            const std::vector<VulkanUniformBuffer>& uniformBuffers,
            const VulkanSceneResources& sceneResources
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

            VulkanDescriptorPool graphicsPool(device, descriptorBindings, uniformBuffers.size());
            VulkanDescriptorSetLayout graphicsSetLayout(device, descriptorBindings);
            VulkanDescriptorSets graphicsSets(device, graphicsPool, graphicsSetLayout, bindingTypes, uniformBuffers.size());

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
                std::vector<VkDescriptorImageInfo> images(sceneResources.getTextureSamplers().size());

                for(size_t j = 0; j != imageInfos.size(); j++) {
                    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    imageInfo.imageView = sceneResources.getTextureImageViews()[images];
                }


            }
        }
};