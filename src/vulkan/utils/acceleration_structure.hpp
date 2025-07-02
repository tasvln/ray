#pragma once

#include <cstdint>

namespace utils
{
    [[nodiscard]]
    constexpr uint64_t alignUp(uint64_t size, uint64_t alignment) noexcept
    {
        return (size + alignment - 1) / alignment * alignment;
    }
}