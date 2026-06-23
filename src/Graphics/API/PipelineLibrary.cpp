/**
 * @file PipelineLibrary.cpp
 * @brief Implementation of the PipelineLibrary class for pipeline caching.
 */
#include "PipelineLibrary.h"
#include <spdlog/spdlog.h>

PipelineLibrary::PipelineLibrary(SDL_GPUDevice* device) : m_Device(device) {}

PipelineLibrary::~PipelineLibrary() {}

GraphicsPipeline* PipelineLibrary::CreatePipeline(const std::string& name, const PipelineConfig& config, SDL_GPUTextureFormat renderTargetFormat) {
    // If a pipeline with this name already exists, destroy it first so that
    // stale shader handles (dangling after the old owning shaders were
    // destroyed) are not carried forward.
    auto it = m_Pipelines.find(name);
    if (it != m_Pipelines.end()) {
        spdlog::info("PipelineLibrary: Replacing existing pipeline '{}'", name);
        m_Pipelines.erase(it);
    }

    // Compile and register the new pipeline
    auto pipeline = std::make_unique<GraphicsPipeline>(m_Device, config, renderTargetFormat);
    GraphicsPipeline* ptr = pipeline.get();
    m_Pipelines[name] = std::move(pipeline);
    
    spdlog::info("PipelineLibrary: Registered new pipeline '{}'", name);
    return ptr;
}

GraphicsPipeline* PipelineLibrary::GetPipeline(const std::string& name) const {
    auto it = m_Pipelines.find(name);
    if (it != m_Pipelines.end()) {
        return it->second.get();
    }
    spdlog::error("PipelineLibrary: Pipeline '{}' not found!", name);
    return nullptr;
}

void PipelineLibrary::Clear() {
    m_Pipelines.clear();
    spdlog::info("PipelineLibrary: All cached pipelines cleared.");
}
