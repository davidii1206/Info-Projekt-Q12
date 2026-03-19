#include "FrameGraph.h"
#include <spdlog/spdlog.h>

FrameGraph::FrameGraph(SDL_GPUDevice* device) : m_Device(device) {}

FrameGraph::~FrameGraph() {}

void FrameGraph::AddPass(const std::string& name, Framebuffer* target, std::function<void(RenderContext&)> func, bool needsDepth, std::function<void(SDL_GPUCommandBuffer*)> preFunc) {
    m_Passes.push_back({ name, PassType::Graphics, target, needsDepth, preFunc, func });
}

void FrameGraph::AddComputePass(const std::string& name, std::function<void(RenderContext&)> func, std::function<void(SDL_GPUCommandBuffer*)> preFunc) {
    m_Passes.push_back({ name, PassType::Compute, nullptr, false, preFunc, func });
}

void FrameGraph::Execute(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* swapchainTexture, uint32_t width, uint32_t height) {
    if (m_Passes.empty()) return;
    
    m_ClearedTargets.clear();
    for (size_t i = 0; i < m_Passes.size(); i++) {
        const auto& passDesc = m_Passes[i];
        
        // Execute pre-pass commands (like PushConstants) OUTSIDE the render/compute pass
        if (passDesc.preExecute) {
            passDesc.preExecute(cmd);
        }

        if (passDesc.type == PassType::Compute) {
            // ... (keep compute logic)
        } 
        else {
            std::vector<SDL_GPUColorTargetInfo> colorTargets;
            SDL_GPUDepthStencilTargetInfo depthTarget = {};
            bool hasDepth = false;

            if (passDesc.target == nullptr) {
                // Direct to Swapchain
                bool alreadyCleared = m_ClearedTargets.find(nullptr) != m_ClearedTargets.end();
                SDL_GPUColorTargetInfo info = {};
                info.texture = swapchainTexture;
                info.clear_color = { 0.1f, 0.1f, 0.1f, 1.0f }; 
                info.load_op = (!alreadyCleared) ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
                info.store_op = SDL_GPU_STOREOP_STORE;
                colorTargets.push_back(info);

                if (passDesc.needsDepth) {
                    if (!m_SwapchainDepth || m_SwapchainDepth->GetWidth() != width || m_SwapchainDepth->GetHeight() != height) {
                        m_SwapchainDepth = std::make_unique<Texture>(m_Device, width, height, SDL_GPU_TEXTUREFORMAT_D32_FLOAT, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET);
                    }

                    depthTarget.texture = m_SwapchainDepth->GetHandle();
                    depthTarget.clear_depth = 0.0f; // 0.0 is Far in Reverse-Z
                    depthTarget.load_op = (!alreadyCleared) ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
                    depthTarget.store_op = SDL_GPU_STOREOP_STORE;
                    hasDepth = true;
                }
                m_ClearedTargets.insert(nullptr);
            } else {
                // To Framebuffer
                bool alreadyCleared = m_ClearedTargets.find(passDesc.target) != m_ClearedTargets.end();
                for (uint32_t j = 0; j < passDesc.target->GetColorTargetCount(); ++j) {
                    SDL_GPUColorTargetInfo info = {};
                    info.texture = passDesc.target->GetColorTarget(j)->GetHandle();
                    info.clear_color = { 0.1f, 0.1f, 0.1f, 0.0f }; 
                    info.load_op = (!alreadyCleared) ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
                    info.store_op = SDL_GPU_STOREOP_STORE;
                    colorTargets.push_back(info);
                }

                if (passDesc.target->GetDepthTarget()) {
                    depthTarget.texture = passDesc.target->GetDepthTarget()->GetHandle();
                    depthTarget.clear_depth = 0.0f; // 0.0 is Far in Reverse-Z
                    depthTarget.load_op = (!alreadyCleared) ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
                    depthTarget.store_op = SDL_GPU_STOREOP_STORE;
                    hasDepth = true;
                }
                m_ClearedTargets.insert(passDesc.target);
            }
            
            SDL_GPURenderPass* sdlPass = SDL_BeginGPURenderPass(cmd, colorTargets.data(), (uint32_t)colorTargets.size(), hasDepth ? &depthTarget : nullptr);
            if (sdlPass) {
                uint32_t passW = passDesc.target ? passDesc.target->GetWidth() : width;
                uint32_t passH = passDesc.target ? passDesc.target->GetHeight() : height;
                SDL_GPUViewport viewport = { 0.0f, 0.0f, (float)passW, (float)passH, 0.0f, 1.0f };
                SDL_SetGPUViewport(sdlPass, &viewport);
                SDL_Rect scissor = { 0, 0, (int)passW, (int)passH };
                SDL_SetGPUScissor(sdlPass, &scissor);

                RenderContext ctx(sdlPass, cmd);
                passDesc.execute(ctx);
                SDL_EndGPURenderPass(sdlPass);
            } else {
                spdlog::error("FrameGraph: Failed to begin render pass '{}'", passDesc.name);
            }
        }
    }
}

void FrameGraph::Reset() {
    m_Passes.clear();
}
