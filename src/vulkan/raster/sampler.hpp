#pragma once

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

#include "device.hpp"

struct VulkanSamplerConfig {
    VulkanSamplerConfig() = default;
    
    // Filtering
    VkFilter magFilter = VK_FILTER_LINEAR;
    VkFilter minFilter = VK_FILTER_LINEAR;
    VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    float mipLodBias = 0.0f;
    float minLod = 0.0f;
    float maxLod = 0.0f;

    // Addressing
    VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    // Anisotropy
    bool anisotropyEnable = true;
    float maxAnisotropy = 16.0f;

    // Border / Comparison
    VkBorderColor borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    bool compareEnable = false;
    VkCompareOp compareOp = VK_COMPARE_OP_ALWAYS;

    // Coordinate system -> tract this apparently most devs overlook this
    bool unnormalizedCoordinates = false;
};


class VulkanSampler {
    public:
        VulkanSampler(VkDevice device, const VulkanSamplerConfig& config): device(device) {
            VkSamplerCreateInfo samplerInfo{};
            samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.magFilter = config.magFilter;
            samplerInfo.minFilter = config.minFilter;

            samplerInfo.addressModeU = config.addressModeU;
            samplerInfo.addressModeV = config.addressModeV;
            samplerInfo.addressModeW = config.addressModeW;

            samplerInfo.anisotropyEnable = config.anisotropyEnable ? VK_TRUE : VK_FALSE;
            samplerInfo.maxAnisotropy = config.maxAnisotropy;

            samplerInfo.borderColor = config.borderColor;
            samplerInfo.unnormalizedCoordinates = config.unnormalizedCoordinates ? VK_TRUE : VK_FALSE;

            samplerInfo.compareEnable = config.compareEnable ? VK_TRUE : VK_FALSE;
            samplerInfo.compareOp = config.compareOp;

            samplerInfo.mipmapMode = config.mipmapMode;
            samplerInfo.mipLodBias = config.mipLodBias;
            samplerInfo.minLod = config.minLod;
            samplerInfo.maxLod = config.maxLod;

            if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
                throw std::runtime_error("failed to create texture sampler");
            }
        }

        ~VulkanSampler() {
            if (sampler != VK_NULL_HANDLE) {
                vkDestroySampler(device, sampler, nullptr);
                sampler = VK_NULL_HANDLE;
            }
        }

        const VkSampler getSampler() const {
            return sampler;
        }

    private:
        VkDevice device;

        VkSampler sampler;
};