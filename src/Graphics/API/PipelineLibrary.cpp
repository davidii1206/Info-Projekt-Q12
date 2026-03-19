#include "PipelineLibrary.h"
#include <spdlog/spdlog.h>

PipelineLibrary::PipelineLibrary(SDL_GPUDevice* device) : m_Device(device) {}

PipelineLibrary::~PipelineLibrary() {}

GraphicsPipeline* PipelineLibrary::CreatePipeline(const std::string& name, const PipelineConfig& config, SDL_GPUTextureFormat renderTargetFormat) {
    if (m_Pipelines.find(name) != m_Pipelines.end()) {
        spdlog::warn("PipelineLibrary: Pipeline '{}' already exists. Returning existing instance.", name);
        return m_Pipelines[name].get();
    }

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
