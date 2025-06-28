#pragma once

#include <glm/glm.hpp>
#include "procedural.hpp"

class VulkanSphere : public VulkanProcedural {
    public:
        VulkanSphere(const glm::vec3& center, float radius) : center(center), radius(radius) {}

        std::pair<glm::vec3, glm::vec3> getBoundingBox() const override {
            return { 
                center - glm::vec3(radius), 
                center + glm::vec3(radius) 
            };
        }

        const glm::vec3& getCenter() const {
            return center;
        }

        float getRadius() const {
            return radius;
        }

    private:
        glm::vec3 center;
        float radius;
}