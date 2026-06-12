/**
 * @file Model.h
 * @brief 3D model management and rendering data.
 */
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
    glm::vec3 position;  /**< Vertex position. */
    glm::vec3 normal;    /**< Vertex normal. */
    glm::vec2 texCoords; /**< Texture coordinates. */
    glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f}; /**< Vertex color. */
    glm::vec3 tangent = {0.0f, 0.0f, 0.0f};     /**< Vertex tangent for normal mapping. */
};

/**
 * @struct Material
 * @brief Simple material definition for GLTF models.
 */
struct Material {
    std::string name; /**< Name of the material. */
    std::shared_ptr<Texture> baseColorTexture; /**< Base color (albedo) texture. */
    std::shared_ptr<Texture> normalTexture;    /**< Normal map texture. */
    std::shared_ptr<Texture> metallicRoughnessTexture; /**< Metallic-Roughness map texture. */
    glm::vec4 baseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f}; /**< Base color constant factor. */
    float metallicFactor = 1.0f;  /**< Metallic constant factor. */
    float roughnessFactor = 1.0f; /**< Roughness constant factor. */
};

/**
 * @struct MeshSection
 * @brief A subset of a mesh with a specific material.
 */
struct MeshSection {
    uint32_t firstIndex;    /**< Starting index in the index buffer. */
    uint32_t indexCount;    /**< Number of indices in this section. */
    uint32_t materialIndex; /**< Index of the material in the materials list. */
};

/**
 * @struct GPUMaterial
 * @brief GPU-side representation of a material for SSBO storage.
 */
struct GPUMaterial {
    glm::vec4 baseColorFactor;    /**< Base color constant factor. */
    float metallicFactor;         /**< Metallic constant factor. */
    float roughnessFactor;        /**< Roughness constant factor. */
    int32_t baseColorTextureIndex; /**< Index in texture array (-1 if none). */
    int32_t normalTextureIndex;    /**< Index in texture array (-1 if none). */
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
    /**
     * @brief Constructs a new Model with provided geometry and materials.
     * @param device Pointer to the active SDL_GPUDevice.
     * @param vertices List of vertices.
     * @param indices List of indices.
     * @param sections List of mesh sections.
     * @param materials List of materials.
     */
    Model(SDL_GPUDevice* device, const std::vector<ModelVertex>& vertices, const std::vector<uint32_t>& indices, const std::vector<MeshSection>& sections, const std::vector<Material>& materials);

    /**
     * @brief Destroys the Model and releases GPU resources.
     */
    ~Model();

    /** @brief Returns the vertex buffer. @return Pointer to GPUBuffer. */
    GPUBuffer* GetVertexBuffer() const { return m_VertexBuffer.get(); }

    /** @brief Returns the index buffer. @return Pointer to GPUBuffer. */
    GPUBuffer* GetIndexBuffer() const { return m_IndexBuffer.get(); }

    /** @brief Returns the material storage buffer (SSBO). @return Pointer to GPUBuffer. */
    GPUBuffer* GetMaterialBuffer() const { return m_MaterialBuffer.get(); }

    /** @brief Returns all mesh sections. @return Const reference to a vector of MeshSection. */
    const std::vector<MeshSection>& GetSections() const { return m_Sections; }

    /** @brief Returns all materials. @return Const reference to a vector of Material. */
    const std::vector<Material>& GetMaterials() const { return m_Materials; }

private:
    std::unique_ptr<GPUBuffer> m_VertexBuffer;  /**< Buffer containing vertex data. */
    std::unique_ptr<GPUBuffer> m_IndexBuffer;   /**< Buffer containing index data. */
    std::unique_ptr<GPUBuffer> m_MaterialBuffer; /**< Buffer containing material data (SSBO). */
    std::vector<MeshSection> m_Sections;        /**< List of mesh sections. */
    std::vector<Material> m_Materials;          /**< List of materials. */
};
