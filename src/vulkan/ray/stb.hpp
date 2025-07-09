#pragma once

#include "device_properties.hpp"
#include "dispatch_table.hpp"
#include "ray_pipeline.hpp"
#include "vulkan/helpers/scene_resources.hpp"
#include "vulkan/raster/buffer.hpp"
#include "vulkan/utils/stb.hpp"

#include <algorithm>
#include <cstring>

struct SBTRegion {
    size_t offset = 0;
    size_t size = 0;
    size_t entrySize = 0;

    [[nodiscard]] VkStridedDeviceAddressRegionKHR toVkRegion(VkDeviceAddress base) const {
        VkStridedDeviceAddressRegionKHR region{};
        region.deviceAddress = base + offset;
        region.stride = entrySize;
        region.size = size;
        return region;
    }
};

class VulkanRaySTB {
    public:
        VulkanRaySTB(
            const VulkanDevice& device,
            const VulkanRayDispatchTable dispatch,
            const VulkanRayPipeline& pipeline,
            const VulkanRayDeviceProperties& props,
            const std::vector<utils::ShaderRecord>& rayGenRecords,
            const std::vector<utils::ShaderRecord>& rayMissRecords,
            const std::vector<utils::ShaderRecord>& rayHitRecords
        ) {
            this->rayGen.entrySize = utils::getRecordSize(props, rayGenRecords);
            this->rayMiss.entrySize = utils::getRecordSize(props, rayMissRecords);
            this->rayHit.entrySize = utils::getRecordSize(props, rayHitRecords);

            this->rayGen.offset = 0;
            this->rayMiss.offset = rayGenRecords.size() * rayGen.entrySize;
            this->rayHit.offset = (rayMiss.offset + rayMissRecords.size()) * rayMiss.entrySize;

            this->rayGen.size = rayGenRecords.size() * rayGen.entrySize;
            this->rayMiss.size = rayMissRecords.size() * rayMiss.entrySize;
            this->rayHit.size = rayHitRecords.size() * rayHit.entrySize;

            // this->rayGen.offset = 0;
            // this->rayGen.size = rayGenRecords.size() * rayGen.entrySize;

            // this->rayMiss.offset = rayGen.offset + rayGen.size;
            // this->rayMiss.size = rayMissRecords.size() * rayMiss.entrySize;

            // this->rayHit.offset = rayMiss.offset + rayMiss.size;
            // this->rayHit.size = rayHitRecords.size() * rayHit.entrySize;

            // // Compute aligned entry sizes
            // rayGen.entrySize = utils::getRecordSize(props, rayGenRecords);
            // rayMiss.entrySize = utils::getRecordSize(props, rayMissRecords);
            // rayHit.entrySize = utils::getRecordSize(props, rayHitRecords);

            // // Compute sizes
            // rayGen.size = rayGenRecords.size() * rayGen.entrySize;
            // rayMiss.size = rayMissRecords.size() * rayMiss.entrySize;
            // rayHit.size = rayHitRecords.size() * rayHit.entrySize;

            // // Compute offsets
            // rayGen.offset = 0;
            // rayMiss.offset = rayGen.offset + rayGen.size;
            // rayHit.offset = rayMiss.offset + rayMiss.size;

            const size_t sbtSize = rayGen.size + rayMiss.size + rayHit.size;

            buffer = std::make_unique<VulkanBuffer>(
                device.getDevice(),
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
                sbtSize
            );

            bufferMemory = std::make_unique<VulkanDeviceMemory>(
                buffer->allocateMemory(
                    device.getPhysicalDevice(),
                    VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                )
            );

            const uint32_t handleSize = props.getShaderGroupHandleSize();
            const size_t groupCount = rayGenRecords.size() + rayMissRecords.size() + rayHitRecords.size();

            std::vector<uint8_t> shaderStorage(groupCount * handleSize);

            if (dispatch.vkGetRayTracingShaderGroupHandlesKHR(
                device.getDevice(),
                pipeline.getPipeline(),
                0,
                static_cast<uint32_t>(groupCount),
                shaderStorage.size(),
                shaderStorage.data()
            ) != VK_SUCCESS) {
                throw std::runtime_error("Failed to get ray tracing shader group handles");
            }

            auto* mapped = static_cast<uint8_t*>(bufferMemory->map(0, sbtSize));
            uint8_t* ptr = mapped;

            ptr += utils::copyShaderRecords(ptr, props, rayGenRecords, rayGen.entrySize, shaderStorage.data());
            ptr += utils::copyShaderRecords(ptr, props, rayMissRecords, rayMiss.entrySize, shaderStorage.data());
            ptr += utils::copyShaderRecords(ptr, props, rayHitRecords, rayHit.entrySize, shaderStorage.data());

            bufferMemory->unMap();
        }

        ~VulkanRaySTB() = default;

        SBTRegion getRayGenRegion() const {
            return rayGen;
        }

        SBTRegion getRayMissRegion() const {
            return rayMiss;
        }

        SBTRegion getRayHitRegion() const {
            return rayHit;
        }

        const VulkanBuffer& getBuffer() const { 
            return *buffer; 
        }

        const VulkanDeviceMemory& getBufferMemory() const { 
            return *bufferMemory; 
        }

         VkDeviceAddress getBaseAddress() const {
            return buffer->getDeviceAddress();
        }

        VkStridedDeviceAddressRegionKHR getRayGenDeviceRegion() const {
            return rayGen.toVkRegion(getBaseAddress());
        }

        VkStridedDeviceAddressRegionKHR getRayMissDeviceRegion() const {
            return rayMiss.toVkRegion(getBaseAddress());
        }

        VkStridedDeviceAddressRegionKHR getRayHitDeviceRegion() const {
            return rayHit.toVkRegion(getBaseAddress());
        }
        
    private:
        std::unique_ptr<VulkanBuffer> buffer;
        std::unique_ptr<VulkanDeviceMemory> bufferMemory;

        SBTRegion rayGen;
        SBTRegion rayMiss;
        SBTRegion rayHit;
};