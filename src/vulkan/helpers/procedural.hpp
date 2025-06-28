// an abstract base class for procedurally-generated geometry

#pragma once

#include <glm/glm.hpp>
#include <utility>

class VulkanProcedural
{
public:

    // VulkanProcedural(const VulkanProcedural&) = delete;
    // VulkanProcedural(VulkanProcedural&&) = delete;
    // VulkanProcedural& operator = (const VulkanProcedural&) = delete;
    // VulkanProcedural& operator = (VulkanProcedural&&) = delete;

    VulkanProcedural() = default;
    virtual ~VulkanProcedural() = default;

    virtual std::pair<glm::vec3, glm::vec3> getBoundingBox() const = 0;
};