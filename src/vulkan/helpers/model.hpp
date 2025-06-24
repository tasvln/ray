#pragma once

#include "vertex.hpp"

//#define TINYOBJLOADER_USE_MAPBOX_EARCUT
#include <tiny_obj_loader.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/hash.hpp>

#include <iostream>
#include <chrono>
#include <vector>
#include <array>
#include <memory>
#include <filesystem>
#include <unordered_map>

struct ModelObject {
    std::vector<VulkanVertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<VulkanMaterial> materials;
    // make this optional
    std::shared_ptr<const VulkanProcedural> procedural;
};

struct VertexHasher {
    size_t operator()(const VulkanVertex& vertex) const {
        // return Combine(...);
    }

    static size_t Combine(size_t hash0, size_t hash1) {
        return hash0 ^ (hash1 + 0x9e3779b9 + (hash0 << 6) + (hash0 >> 2));
    }
};

// std::shared_ptr<Model>

class VulkanModel{
    public:
        VulkanModel(std::vector<VulkanVertex> vertices,
            std::vector<uint32_t> indices,
            std::vector<VulkanMaterial> materials,
            std::shared_ptr<const VulkanProcedural> procedural = nullptr) : 
            model{
                std::move(vertices), 
                std::move(indices), 
                std::move(materials), 
                std::move(procedural)
            }
        {}


        ~VulkanModel() {

        }

        VulkanModel loadModel(const std::string& filename) {

            const auto timer = std::chrono::high_resolution_clock::now();
	        const std::string materialPath = std::filesystem::path(filename).parent_path().string();

            tinyobj::ObjReader reader;

            if (!reader.ParseFromFile(filename))
                std::runtime_error("failed to load model '" + filename + "':\n" + reader.Error());

            if (!reader.Warning().empty())
                std::cout << "TinyObjReader: " << reader.Warning();

            auto materials = loadMaterials(reader);
        }

        static std::vector<VulkanMaterial> loadMaterials(const tinyobj::ObjReader& reader) {
            std::vector<VulkanMaterial> materials;

            for (const auto& material : objReader.GetMaterials())
            {
                VulkanMaterial tempMaterial{};

                tempMaterial.Diffuse = glm::vec4(material.diffuse[0], material.diffuse[1], material.diffuse[2], 1.0);
                tempMaterial.DiffuseTextureId = -1;

                materials.emplace_back(tempMaterial);
            }
            
        }

        void setMaterial(const VulkanMaterial& material) {

        }
        
        void transform(const glm::mat4& transform) {
            
        }


        const std::vector<VulkanVertex>& getVertices() const {
            return model.vertices;
        }

        const std::vector<uint32_t>& getIndices() const {
            return model.indices;
        }

        const std::vector<VulkanMaterial>& getMaterial() const {
            return model.materials;
        }

        const VulkanProcedural* getProcedural() const {
            return model.procedural.get();
        }

        uint32_t getNumOfIndices() const {
            return model.indices.size();
        }

        uint32_t getNumOfVertices() const {
            return model.vertices.size();
        }

        uint32_t getNumOfOfMaterials() const {
            return model.materials.size();
        }

    private:

    ModelObject model;
        
};