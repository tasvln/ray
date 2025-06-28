#pragma once

#include <glm/glm.hpp>
#include <cstdint>

// explore material hash -> you don't hash materials unless you want to quickly look them up or deduplicate them in hash-based containers.

class VulkanMaterial {
public:
    enum class MaterialType : uint32_t {
        LAMBERTIAN    = 0,
        METALLIC      = 1,
        DIELECTRIC    = 2,
        ISOTROPIC     = 3,
        DIFFUSE_LIGHT = 4,
        CUSTOM        = 5
    };

    VulkanMaterial() = default;

    // Lambertian: diffuse color only, no specular/emission
    static VulkanMaterial lambertian(const glm::vec3& color, int32_t textureId = -1) {
        VulkanMaterial mat{};
        mat.diffuse = glm::vec4(color, 1.0f);
        mat.specular = glm::vec4(0.0f);     // no specular
        mat.extraParams = glm::vec4(0.0f);  // roughness=0, metallic=0, opacity=1 set later
        mat.extraParams.z = 1.0f;            // opacity = 1
        mat.emission = glm::vec4(0.0f);
        mat.type = MaterialType::LAMBERTIAN;
        mat.textureId = textureId;
        return mat;
    }

    // Metallic: diffuse + roughness + metallic + opacity
    static VulkanMaterial metallic(const glm::vec3& color, float roughness, float opacity = 1.0f, int32_t textureId = -1) {
        VulkanMaterial mat{};
        mat.diffuse = glm::vec4(color, 1.0f);
        mat.specular = glm::vec4(1.0f);      // white specular highlight
        mat.extraParams = glm::vec4(roughness, 1.0f, opacity, 0.0f);  // metallic=1
        mat.emission = glm::vec4(0.0f);
        mat.type = MaterialType::METALLIC;
        mat.textureId = textureId;
        return mat;
    }

    // Dielectric: refraction index + opacity, emission off
    static VulkanMaterial dielectric(float refractionIndex, float opacity = 1.0f, int32_t textureId = -1) {
        VulkanMaterial mat{};
        mat.diffuse = glm::vec4(0.7f, 0.7f, 1.0f, 1.0f);
        mat.specular = glm::vec4(1.0f);
        mat.extraParams = glm::vec4(0.0f, 0.0f, opacity, refractionIndex);
        mat.emission = glm::vec4(0.0f);
        mat.type = MaterialType::DIELECTRIC;
        mat.textureId = textureId;
        return mat;
    }

    // Isotropic: similar to Lambertian but separate type, no emission
    static VulkanMaterial isotropic(const glm::vec3& color, float opacity = 1.0f, int32_t textureId = -1) {
        VulkanMaterial mat{};
        mat.diffuse = glm::vec4(color, 1.0f);
        mat.specular = glm::vec4(0.0f);
        mat.extraParams = glm::vec4(0.0f, 0.0f, opacity, 0.0f);
        mat.emission = glm::vec4(0.0f);
        mat.type = MaterialType::ISOTROPIC;
        mat.textureId = textureId;
        return mat;
    }

    // Diffuse light: no specular, emission on
    static VulkanMaterial diffuseLight(const glm::vec3& emissionColor, int32_t textureId = -1) {
        VulkanMaterial mat{};
        mat.diffuse = glm::vec4(0.0f);       // no diffuse
        mat.specular = glm::vec4(0.0f);
        mat.extraParams = glm::vec4(0.0f);
        mat.emission = glm::vec4(emissionColor, 1.0f);
        mat.type = MaterialType::DIFFUSE_LIGHT;
        mat.textureId = textureId;
        return mat;
    }

    // Material data:
    glm::vec4 diffuse{};     // base color + alpha
    glm::vec4 specular{};    // specular color + alpha -> optional?
    glm::vec4 extraParams{}; // (x) roughness, (y) metallic (0 or 1), (z) opacity, (w) refractionIndex or unused -> optional?
    glm::vec4 emission{};    // emissive color + alpha

    MaterialType type = MaterialType::LAMBERTIAN;

    int32_t textureId;
};

