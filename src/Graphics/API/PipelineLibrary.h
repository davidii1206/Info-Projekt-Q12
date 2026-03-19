#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include "GraphicsPipeline.h"

/**
 * @class PipelineLibrary
 * @brief Manages the lifecycle and caching of GraphicsPipelines.
 * 
 * This library allows developers to register pipelines under unique names
 * and retrieve them during the rendering phase, ensuring that expensive
 * pipeline state objects are only created once.
 */
class PipelineLibrary {
public:
    PipelineLibrary(SDL_GPUDevice* device);
    ~PipelineLibrary();

    /**
     * @brief Registers a new pipeline in the library.
     * @param name Unique identifier for the pipeline.
     * @param config The configuration used to build the pipeline.
     * @param renderTargetFormat Pixel format of the target (usually swapchain).
     * @return Pointer to the created pipeline.
     */
    GraphicsPipeline* CreatePipeline(const std::string& name, const PipelineConfig& config, SDL_GPUTextureFormat renderTargetFormat);

    /**
     * @brief Retrieves a previously registered pipeline.
     * @param name The unique name of the pipeline.
     * @return Pointer to the pipeline, or nullptr if not found.
     */
    GraphicsPipeline* GetPipeline(const std::string& name) const;

private:
    SDL_GPUDevice* m_Device;
    std::unordered_map<std::string, std::unique_ptr<GraphicsPipeline>> m_Pipelines;
};
