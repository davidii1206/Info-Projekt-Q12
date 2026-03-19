#pragma once
#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "API/GPUBuffer.h"
#include "API/Texture.h"

/**
 * @struct ModelVertex
 * @brief Standard vertex layout for GLTF-loaded models.
 */
struct ModelVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords;
    glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec3 tangent = {0.0f, 0.0f, 0.0f};
};

/**
 * @struct Material
 * @brief Simple material definition for GLTF models.
 */
struct Material {
    std::string name;
    std::shared_ptr<Texture> baseColorTexture;
    std::shared_ptr<Texture> normalTexture;
    std::shared_ptr<Texture> metallicRoughnessTexture;
    glm::vec4 baseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
};

/**
 * @struct MeshSection
 * @brief A subset of a mesh with a specific material.
 */
struct MeshSection {
    uint32_t firstIndex;
    uint32_t indexCount;
    uint32_t materialIndex;
};

/**
 * @struct GPUMaterial
 * @brief GPU-side representation of a material for SSBO storage.
 */
struct GPUMaterial {
    glm::vec4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    int32_t baseColorTextureIndex; // -1 if no texture
    int32_t normalTextureIndex;    // -1 if no texture
};

/**
 * @class Model
 * @brief Represents a 3D model with one or more meshes and materials.
 * 
 * Performance:
 * - Uses indexed drawing with GPU buffers.
 * - Centralized vertex and index buffers for all meshes in the model.
 * - Materials are stored in an SSBO for efficient access.
 */
class Model {
public:
    Model(SDL_GPUDevice* device, const std::vector<ModelVertex>& vertices, const std::vector<uint32_t>& indices, const std::vector<MeshSection>& sections, const std::vector<Material>& materials);
    ~Model();

    /** @brief Returns the vertex buffer. */
    GPUBuffer* GetVertexBuffer() const { return m_VertexBuffer.get(); }

    /** @brief Returns the index buffer. */
    GPUBuffer* GetIndexBuffer() const { return m_IndexBuffer.get(); }

    /** @brief Returns the material storage buffer (SSBO). */
    GPUBuffer* GetMaterialBuffer() const { return m_MaterialBuffer.get(); }

    /** @brief Returns all mesh sections. */
    const std::vector<MeshSection>& GetSections() const { return m_Sections; }

    /** @brief Returns all materials. */
    const std::vector<Material>& GetMaterials() const { return m_Materials; }

private:
    std::unique_ptr<GPUBuffer> m_VertexBuffer;
    std::unique_ptr<GPUBuffer> m_IndexBuffer;
    std::unique_ptr<GPUBuffer> m_MaterialBuffer;
    std::vector<MeshSection> m_Sections;
    std::vector<Material> m_Materials;
};
