#pragma once

#include "descriptor_pool.hpp"

#include <iostream>
#include <vector>
#include <stdexcept>
#include <map>

class VulkanDescriptorSets{
    public:
        VulkanDescriptorSets(const VkDevice& device, const VulkanDescriptorPool& descriptorPool, const VulkanDescriptorSetLayout& layout, std::map<uint32_t, VkDescriptorType> bindingTypes, const size_t size): device(device), bindingTypes(bindingTypes) {
	        std::vector<VkDescriptorSetLayout> layouts(size, layout.getLayout());

            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = descriptorPool.getPool();
            allocInfo.descriptorSetCount = size;
            allocInfo.pSetLayouts = layouts.data();

            sets.resize(size);

            if (vkAllocateDescriptorSets(device, &allocInfo, sets.data()) != VK_SUCCESS) {
                throw std::runtime_error("Failed to allocate fullscreen descriptor sets!");
            }
        }

        // Bind Buffers
        VkWriteDescriptorSet bind(size_t index, uint32_t binding, const VkDescriptorBufferInfo& bufferInfo, uint32_t count = 1) const {
            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = sets[index];
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = getBindingType(binding);
            descriptorWrite.descriptorCount = count;
            descriptorWrite.pBufferInfo = &bufferInfo;

            return descriptorWrite;
        }

        // Bind Image Storage
        VkWriteDescriptorSet bind(size_t index, uint32_t binding, const VkDescriptorImageInfo& imageInfo, uint32_t count = 1) const {
            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = sets[index];
            descriptorWrite.dstBinding = binding;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = getBindingType(binding);
            descriptorWrite.descriptorCount = count;
            descriptorWrite.pImageInfo = &imageInfo;

            return descriptorWrite;
        }

        // Bind Accelerator Struture
        VkWriteDescriptorSet bind(size_t index, uint32_t binding, const VkWriteDescriptorSetAccelerationStructureKHR& structureInfo, uint32_t count = 1) const {
            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = sets[index];
            descriptorWrite.dstBinding = binding;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = getBindingType(binding);
            descriptorWrite.descriptorCount = count;
            descriptorWrite.pNext = &structureInfo;

            return descriptorWrite;
        }

        void updateDescriptors(const std::vector<VkWriteDescriptorSet>& descriptorWrites) {
            vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }

        const std::vector<VkDescriptorSet>& getSets() const {
            return sets;
        }

        VkDescriptorSet getSet(size_t index) const {
            return sets[index];
        }

    private:
        VkDevice device = VK_NULL_HANDLE;
        const std::map<uint32_t, VkDescriptorType> bindingTypes;
        
        std::vector<VkDescriptorSet> sets;

        VkDescriptorType getBindingType(uint32_t binding) const {
            const auto it = bindingTypes.find(binding);

            if (it == bindingTypes.end())
            {
                throw std::invalid_argument("binding not found");
            }

            return it->second;
        }
};