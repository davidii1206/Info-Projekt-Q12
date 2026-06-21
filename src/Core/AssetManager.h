/**
 * @file AssetManager.h
 * @brief Singleton-style manager for loading and caching engine assets.
 */

#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <spdlog/spdlog.h>
#include <SDL3/SDL_gpu.h>
#include "Graphics/API/Texture.h"
#include "Graphics/Model.h"
#include "Graphics/Lights.h"

/**
 * @struct MeshInstance
 * @brief Represents an instance of a mesh within a scene.
 */
struct MeshInstance {
    uint32_t firstSection; ///< Starting index of the mesh sections in this instance.
    uint32_t sectionCount; ///< Number of sections in this instance.
    glm::mat4 transform;   ///< Transformation matrix for this instance.
};

/**
 * @struct SceneData
 * @brief Container for models, lights, and mesh instances loaded from a GLTF file.
 *
 * cpuVertices / cpuIndices retain the raw CPU-side geometry so that callers can
 * build physics mesh shapes without re-parsing the file.  They are populated by
 * LoadGLTF() and cached alongside the GPU model.  Pass them to
 * MeshCollisionBuilder::Build() to create a Jolt MeshShape for static collision.
 */
struct SceneData {
    std::shared_ptr<Model> model; ///< Shared pointer to the loaded Model.
    std::vector<Light> lights;    ///< List of lights found in the scene.
    std::vector<MeshInstance> meshInstances; ///< List of mesh instances in the scene.

    /// CPU-seitige Vertex-Positionen - für die Konstruktion der physikalischen Mesh-Shapes beibehalten.
    std::vector<ModelVertex> cpuVertices;
    /// CPU-seitige Triangle-Indices - für die Konstruktion der physikalischen Mesh-Shapes beibehalten.
    std::vector<uint32_t>    cpuIndices;
};

/**
 * @class AssetManager
 * @brief Singleton-style manager for loading and caching engine assets.
 */
class AssetManager {
public:
    /**
     * @brief Initializes the AssetManager with the active GPU device.
     * @param device The SDL_GPUDevice to use for resource creation.
     */
    static void Init(SDL_GPUDevice* device);

    /**
     * @brief Clears all cached assets and logs shutdown.
     */
    static void Shutdown();

    /**
     * @brief Loads or retrieves a cached texture.
     * @param filePath Path to the image file.
     * @param filter The filtering mode to use.
     * @return Shared pointer to the Texture object.
     */
    static std::shared_ptr<Texture> LoadTexture(const std::string& filePath, TextureFilter filter = TextureFilter::Linear);

    /**
     * @brief Loads or retrieves a cached texture from raw pixel data.
     * @param name Unique name for the texture cache.
     * @param pixels Pointer to the RGBA8 pixel data.
     * @param width Width in pixels.
     * @param height Height in pixels.
     * @param filter The filtering mode to use.
     * @return Shared pointer to the Texture object.
     */
    static std::shared_ptr<Texture> LoadTexture(const std::string& name, unsigned char* pixels, uint32_t width, uint32_t height, TextureFilter filter = TextureFilter::Linear);

    /**
     * @brief Loads or retrieves a cached GLTF model.
     * @param filePath Path to the .gltf or .glb file.
     * @return Shared pointer to the Model object.
     */
    static std::shared_ptr<Model> LoadModel(const std::string& filePath);

    /**
     * @brief Loads a full asset (GLTF) including models and lights.
     * @param filePath Path to the .gltf or .glb file.
     * @return SceneData containing the model and all lights found in the file.
     */
    static const SceneData& LoadGLTF(const std::string& filePath);

    /**
     * @brief Builds a Model from CPU-generated geometry and caches it under a
     *        synthetic key so it can be referenced by ModelComponent like any
     *        GLTF-loaded asset (e.g. a procedurally generated terrain mesh).
     * @param key Synthetic cache key (e.g. "procedural://terrain").
     * @param vertices CPU-side vertex buffer.
     * @param indices CPU-side index buffer (triangle list).
     * @param sections Mesh sections (material groups).
     * @param materials Materials referenced by the sections.
     * @return The cached SceneData, ready for ModelComponent lookup.
     */
    static const SceneData& RegisterProceduralScene(
        const std::string& key,
        std::vector<ModelVertex> vertices,
        std::vector<uint32_t> indices,
        const std::vector<MeshSection>& sections,
        const std::vector<Material>& materials);

    /** @brief Returns a simple procedural unit cube. */
    static std::shared_ptr<Model> GetFallbackModel();

    /** @brief Returns a 1x1 white texture. */
    static std::shared_ptr<Texture> GetFallbackTexture();

private:
    static SDL_GPUDevice* s_Device; ///< SDL GPU device used for resource allocation.
    static std::unordered_map<std::string, std::shared_ptr<Texture>> s_Textures; ///< Cache for loaded textures.
    static std::unordered_map<std::string, SceneData> s_Scenes; ///< Cache for loaded GLTF scenes.
    static std::shared_ptr<Model> s_FallbackModel; ///< Procedural fallback cube model.
    static std::shared_ptr<Texture> s_FallbackTexture; ///< Procedural white fallback texture.
};
