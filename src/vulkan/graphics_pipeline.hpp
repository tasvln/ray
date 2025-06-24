#pragma once

#include "swapchain.hpp"
#include "pipeline_layout.hpp"
#include "descriptor_sets.hpp"
#include "descriptor_pool.hpp"
#include "descriptorset_layout.hpp"
#include "helpers/vertex.hpp"
#include "helpers/uniform_buffer.hpp"

#include <iostream>
#include <vector>
#include <stdexcept>
#include <map>
#include <utility>

// TODO: add depth

class VulkanGraphicsPipeline{
    public:
        VulkanGraphicsPipeline(
            const VkDevice& device, 
            const VulkanSwapChain& swapchain, 
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
            const std::vector<VulkanUniformBuffer>& uniformBuffers
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

            VkPipelineMultisampleStateCreateInfo multisampling{};
            multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multisampling.sampleShadingEnable = VK_FALSE;
            multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

            VkPipelineColorBlendAttachmentState colorBlendAttachment{};
            colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

            VkPipelineColorBlendStateCreateInfo colorBlending{};
            colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlending.logicOpEnable = VK_FALSE;
            colorBlending.logicOp = VK_LOGIC_OP_COPY;   
            colorBlending.attachmentCount = 1;
            colorBlending.pAttachments = &colorBlendAttachment;

            std::vector<DescriptorBinding> descriptorBindings =
            {
                {0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}, // uniform buffer
                {1, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT }, // Albedo or diffuse texture
                {2, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}, // storage buffer
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

        }
};