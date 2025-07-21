#pragma once

#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>

#include "vulkan/ray/device_properties.hpp"

namespace utils
{
    struct ShaderRecord {
        uint32_t groupIndex;
        std::vector<uint8_t> shaderParams;
    };

    inline size_t sbtAlignUp(uint64_t size, uint64_t alignment)
    {
        return (size + alignment - 1) & ~(alignment - 1);
    }

    size_t getRecordSize(
        const VulkanRayDeviceProperties& props,
        const std::vector<ShaderRecord>& records
    ) {
        size_t maxParams = 0;

        for (const auto& rec : records) {
            maxParams = std::max(maxParams, rec.shaderParams.size());
        }

        return alignUp(props.getShaderGroupHandleSize() + maxParams, props.getShaderGroupBaseAlignment());
    }

    size_t copyShaderRecords(
        uint8_t* dst, // -> Start of the memory where records will be written
        const VulkanRayDeviceProperties& props, // -> Used to query the shader group handle size
        const std::vector<ShaderRecord>& records, // -> The list of shader records to write
        size_t entrySize, // -> The total size (with alignment) of each record
        const uint8_t* handleStorage // -> Raw pointer to the shader group handles retrieved earlier
    ) {
        const auto handleSize = props.getShaderGroupHandleSize();
        uint8_t* writePtr = dst;

        for (const auto& record : records) {
            std::memcpy(writePtr, handleStorage + record.groupIndex * handleSize, handleSize);
            std::memcpy(writePtr + handleSize, record.shaderParams.data(), record.shaderParams.size());
            writePtr += entrySize;
        }

        return records.size() * entrySize;
    }
}