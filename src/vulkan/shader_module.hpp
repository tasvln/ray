#pragma once

#include <vulkan/vulkan.hpp>

#include <iostream>
#include <vector>
#include <fstream>
#include <stdexcept>

class VulkanShaderModule {
    public: 
        VulkanShaderModule(const VkDevice& device, const std::string& filename): device(device) {
            auto shaderCode = readFile(filename);
            shaderModule = createShaderModule(shaderCode);
        }

        ~VulkanShaderModule() {
            if (shaderModule != nullptr) {
                vkDestroyShaderModule(device, shaderModule, nullptr);
                shaderModule = nullptr;
            }
        }

        VkPipelineShaderStageCreateInfo createShaderStage(VkShaderStageFlagBits stage) const
        {
            VkPipelineShaderStageCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            createInfo.stage = stage;
            createInfo.module = shaderModule;
            createInfo.pName = "main";

            return createInfo;
        }

        const VkShaderModule& getShaderModule() {
            return shaderModule;
        }

    private:
        VkDevice device = VK_NULL_HANDLE;
        VkShaderModule shaderModule = VK_NULL_HANDLE;
        
        VkShaderModule createShaderModule(const std::vector<char>& code) {
            VkShaderModuleCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            createInfo.codeSize = code.size();
            createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

            VkShaderModule shaderModule;
            if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
                throw std::runtime_error("failed to create shader module!");
            }

            return shaderModule;
        }

        std::vector<char> readFile(const std::string& filename) {
            std::ifstream file(filename, std::ios::ate | std::ios::binary);

            if (!file.is_open()) {
                throw std::runtime_error("failed to open file!'" + filename + "");
            }

            size_t fileSize = (size_t)file.tellg();
            std::vector<char> buffer(fileSize);

            file.seekg(0);
            file.read(buffer.data(), fileSize);

            file.close();

            return buffer;
        }
};