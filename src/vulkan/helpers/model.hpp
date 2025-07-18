#pragma once

#include "vertex.hpp"
#include "material.hpp"
#include "procedural.hpp"

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
        size_t hash = std::hash<float>()(vertex.position.x);

        hash = combine(hash, std::hash<float>()(vertex.position.y));
        hash = combine(hash, std::hash<float>()(vertex.position.z));

        hash = combine(hash, std::hash<float>()(vertex.normal.x));
        hash = combine(hash, std::hash<float>()(vertex.normal.y));
        hash = combine(hash, std::hash<float>()(vertex.normal.z));

        hash = combine(hash, std::hash<float>()(vertex.texCoord.x));
        hash = combine(hash, std::hash<float>()(vertex.texCoord.y));

        hash = combine(hash, std::hash<int>()(vertex.materialIndex));

        return hash;
    }

    // technique by Boost and Bob Jenkins -> hash tables -> magic constant (0x9e3779b9 == 2,654,435,769)
    static size_t combine(size_t hash0, size_t hash1) {
        return hash0 ^ (hash1 + 0x9e3779b9 + (hash0 << 6) + (hash0 >> 2));
    }
};


// std::shared_ptr<Model>

class VulkanModel{
    public:
        VulkanModel(const std::string& filename) {
            const auto timer = std::chrono::high_resolution_clock::now();

            const std::string materialPath = std::filesystem::path(filename).parent_path().string();
            tinyobj::ObjReader reader;

            if (!reader.ParseFromFile(filename))
                throw std::runtime_error("failed to load model '" + filename + "':\n" + reader.Error());

            if (!reader.Warning().empty())
                std::cout << "TinyObjReader warning: " << reader.Warning();

            auto materials = loadMaterials(reader);

            std::vector<VulkanVertex> vertices;
            std::vector<uint32_t> indices;
            std::unordered_map<VulkanVertex, uint32_t> uniqueVertices;

            processMeshData(reader, vertices, indices, uniqueVertices);

            if (reader.GetAttrib().normals.empty())
                generateSmoothNormals(vertices, indices);

            model = ModelObject {
                std::move(vertices),
                std::move(indices),
                std::move(materials),
                nullptr
            };

            const auto elapsed = std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - timer).count();
        }

        // VulkanModel(
        //     std::vector<VulkanVertex> vertices,
        //     std::vector<uint32_t> indices,
        //     std::vector<VulkanMaterial> materials,
        //     std::shared_ptr<const VulkanProcedural> procedural = nullptr
        // ): 
        //     model{
        //         std::move(vertices), 
        //         std::move(indices), 
        //         std::move(materials), 
        //         std::move(procedural)
        //     }
        // {}

        // bool operator==(const VulkanVertex& lhs, const VulkanVertex& rhs) {
        //     return lhs.position == rhs.position &&
        //         lhs.normal == rhs.normal &&
        //         lhs.texCoord == rhs.texCoord &&
        //         lhs.materialIndex == rhs.materialIndex;
        // }

        ~VulkanModel() = default;

        // VulkanModel loadModel(const std::string& filename) {
        //     const auto timer = std::chrono::high_resolution_clock::now();

	    //     const std::string materialPath = std::filesystem::path(filename).parent_path().string();

        //     tinyobj::ObjReader reader;

        //     if (!reader.ParseFromFile(filename))
        //         throw std::runtime_error("failed to load model->loadModel() '" + filename + "':\n" + reader.Error());

        //     if (!reader.Warning().empty())
        //         std::cout << "TinyObjReader->loadModel(): " << reader.Warning();

        //     auto materials = loadMaterials(reader);

        //     std::vector<VulkanVertex> vertices;
        //     std::vector<uint32_t> indices;
        //     std::unordered_map<VulkanVertex, uint32_t> uniqueVertices;

        //     processMeshData(reader, vertices, indices, uniqueVertices);

        //     if (reader.GetAttrib().normals.empty())
        //         generateSmoothNormals(vertices, indices);
            
        //     const auto elapsed = std::chrono::duration<float, std::chrono::seconds::period>(std::chrono::high_resolution_clock::now() - timer).count();

        //     return VulkanModel(vertices, indices, materials, nullptr);
        // }

        std::vector<VulkanMaterial> loadMaterials(const tinyobj::ObjReader& reader) {
            std::vector<VulkanMaterial> materials;

            for (const auto& mat : reader.GetMaterials()) {
                VulkanMaterial material{};

                // Diffuse color with alpha = 1
                material.diffuse = glm::vec4(mat.diffuse[0], mat.diffuse[1], mat.diffuse[2], 1.0f);

                // Specular color (set alpha to 1 for alignment if needed)
                material.specular = glm::vec4(mat.specular[0], mat.specular[1], mat.specular[2], 1.0f);

                // Emission color (if supported)
                material.emission = glm::vec4(mat.emission[0], mat.emission[1], mat.emission[2], 1.0f);

                // Roughness, metallic, opacity in a packed vec4
                material.extraParams = glm::vec4(
                    1.0f - mat.shininess / 1000.0f, // approximate roughness from shininess
                    0.0f,                           // metallic (TinyObj doesn't provide this)
                    mat.dissolve,                  // opacity
                    0.0f                           // unused
                );

                // Texture ID placeholder (you can assign actual IDs later when loading textures)
                material.textureId = -1;

                materials.emplace_back(material);
            }

            // Fallback if no materials were defined
            if (materials.empty()) {
                VulkanMaterial defaultMat{};
                defaultMat.diffuse = glm::vec4(0.7f, 0.7f, 0.7f, 1.0f);
                defaultMat.specular = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
                defaultMat.extraParams = glm::vec4(0.5f, 0.0f, 1.0f, 0.0f); // roughness, metallic, opacity
                defaultMat.emission = glm::vec4(0.0f);
                defaultMat.textureId = -1;

                materials.emplace_back(defaultMat);
            }

            return materials;
        }

        void processMeshData(const tinyobj::ObjReader& reader,
                            std::vector<VulkanVertex>& vertices,
                            std::vector<uint32_t>& indices,
                            std::unordered_map<VulkanVertex, uint32_t>& uniqueVertices) {
            
            const auto& attrib = reader.GetAttrib();
            const auto& shapes = reader.GetShapes();

            size_t faceVertexOffset = 0;

            for (const auto& shape : shapes) {
                const auto& mesh = shape.mesh;
                const auto& indicesRaw = mesh.indices;

                for (size_t i = 0; i < indicesRaw.size(); ++i) {
                    const auto& index = indicesRaw[i];
                    VulkanVertex vertex{};

                    // position
                    if (index.vertex_index >= 0) {
                        vertex.position = {
                            attrib.vertices[3 * index.vertex_index + 0],
                            attrib.vertices[3 * index.vertex_index + 1],
                            attrib.vertices[3 * index.vertex_index + 2]
                        };
                    }

                    // normal
                    if (!attrib.normals.empty() && index.normal_index >= 0) {
                        vertex.normal = {
                            attrib.normals[3 * index.normal_index + 0],
                            attrib.normals[3 * index.normal_index + 1],
                            attrib.normals[3 * index.normal_index + 2]
                        };
                    }

                    // Texture Coordinate
                    if (!attrib.texcoords.empty() && index.texcoord_index >= 0) {
                        vertex.texCoord = {
                            attrib.texcoords[2 * index.texcoord_index + 0],
                            1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                        };
                    }

                    // Material Index (per face)
                    const int faceIndex = static_cast<int>(i / 3);
                    vertex.materialIndex = mesh.material_ids.empty() ? 0 : std::max(mesh.material_ids[faceIndex], 0);

                    // Deduplication
                    if (uniqueVertices.count(vertex) == 0) {
                        uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                        vertices.push_back(vertex);
                    }

                    indices.push_back(uniqueVertices[vertex]);
                }
            }
        }

        void generateSmoothNormals(std::vector<VulkanVertex>& vertices, const std::vector<uint32_t>& indices)
        {
            // Reset all normals to zero
            for (auto& vertex : vertices)
            {
                vertex.normal = glm::vec3(0.0f);
            }

            // Accumulate face normals for each vertex
            for (size_t i = 0; i < indices.size(); i += 3)
            {
                uint32_t i0 = indices[i + 0];
                uint32_t i1 = indices[i + 1];
                uint32_t i2 = indices[i + 2];

                const glm::vec3& p0 = vertices[i0].position;
                const glm::vec3& p1 = vertices[i1].position;
                const glm::vec3& p2 = vertices[i2].position;

                glm::vec3 edge1 = p1 - p0;
                glm::vec3 edge2 = p2 - p0;
                glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));

                vertices[i0].normal += normal;
                vertices[i1].normal += normal;
                vertices[i2].normal += normal;
            }

            // Normalize the accumulated normals
            for (auto& vertex : vertices)
            {
                vertex.normal = glm::normalize(vertex.normal);
            }
        }


        void setMaterial(const VulkanMaterial& material) {
            if (model.materials.size() != 1) {
                std::runtime_error("cannot change material on a multi-material model");
            }

            model.materials[0] = material;
        }
        
        void transform(const glm::mat4& transform) {
            const auto revTransform = glm::inverseTranspose(transform);
            
            for(auto& vertex: model.vertices) {
                vertex.position = transform * glm::vec4(vertex.position, 1);
                vertex.normal = revTransform * glm::vec4(vertex.normal, 0);
            }
        }

        const std::vector<VulkanVertex>& getVertices() const {
            return model.vertices;
        }

        const std::vector<uint32_t>& getIndices() const {
            return model.indices;
        }

        const std::vector<VulkanMaterial>& getMaterials() const {
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