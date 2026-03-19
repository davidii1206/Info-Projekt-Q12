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
 * @class AssetManager
 * @brief Singleton-style manager for loading and caching engine assets.
 * 
 * Performance:
 * - Caches textures and models to avoid redundant I/O and GPU uploads.
 * - Simple API for fast access during runtime.
 * 
 * Ease of Use:
 * - Centralized management of GPU resources.
 * - Automatic cleanup of assets during shutdown.
 */
struct MeshInstance {
    uint32_t firstSection;
    uint32_t sectionCount;
    glm::mat4 transform;
};

struct SceneData {
    std::shared_ptr<Model> model;
    std::vector<Light> lights;
    std::vector<MeshInstance> meshInstances;
};

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
     * @brief Loads a full scene including models and lights.
     * @param filePath Path to the .gltf or .glb file.
     * @return SceneData containing the model and all lights found in the file.
     */
    static SceneData LoadScene(const std::string& filePath);

    /** @brief Returns a simple procedural unit cube. */
    static std::shared_ptr<Model> GetFallbackModel();

    /** @brief Returns a 1x1 white texture. */
    static std::shared_ptr<Texture> GetFallbackTexture();

private:
    static SDL_GPUDevice* s_Device;
    static std::unordered_map<std::string, std::shared_ptr<Texture>> s_Textures;
    static std::unordered_map<std::string, SceneData> s_Scenes;
    static std::shared_ptr<Model> s_FallbackModel;
    static std::shared_ptr<Texture> s_FallbackTexture;
};
