/**
 * @file GraphicsPipeline.cpp
 * @brief Implementation of the GraphicsPipeline class for GPU state management.
 */
#include "GraphicsPipeline.h"
#include <spdlog/spdlog.h>

GraphicsPipeline::GraphicsPipeline(SDL_GPUDevice* device, const PipelineConfig& config, SDL_GPUTextureFormat renderTargetFormat)
    : m_Device(device), m_Pipeline(nullptr) 
{
    if (!device) {
        spdlog::error("GraphicsPipeline: Device is NULL!");
        return;
    }

    if (!config.vertexShader || !config.vertexShader->GetHandle()) {
        spdlog::error("GraphicsPipeline: Vertex shader is invalid!");
        return;
    }

    if (!config.fragmentShader || !config.fragmentShader->GetHandle()) {
        spdlog::error("GraphicsPipeline: Fragment shader is invalid!");
        return;
    }

    // Map vertex attributes to SDL's format
    std::vector<SDL_GPUVertexAttribute> sdlAttributes;
    sdlAttributes.reserve(config.vertexAttributes.size());
    for (const auto& attr : config.vertexAttributes) {
        SDL_GPUVertexAttribute sdlAttr;
        sdlAttr.location = attr.location;
        sdlAttr.buffer_slot = 0;
        sdlAttr.format = attr.format;
        sdlAttr.offset = attr.offset;
        sdlAttributes.push_back(sdlAttr);
    }

    // Set up vertex buffer description
    SDL_GPUVertexBufferDescription vertexBufferDesc = {};
    vertexBufferDesc.slot = 0;
    vertexBufferDesc.pitch = config.vertexStride;
    vertexBufferDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vertexBufferDesc.instance_step_rate = 0;

    // Set up color target descriptions and blending state
    std::vector<SDL_GPUColorTargetDescription> colorTargetDescs;
    if (!config.colorTargetFormats.empty()) {
        for (auto format : config.colorTargetFormats) {
            SDL_GPUColorTargetDescription desc = {};
            desc.format = format;
            desc.blend_state.enable_blend = config.enableBlending;
            if (config.enableBlending) {
                desc.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
                desc.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
                desc.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
                desc.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
                desc.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
                desc.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
            }
            desc.blend_state.color_write_mask = 0xF;
            colorTargetDescs.push_back(desc);
        }
    } else {
        if (renderTargetFormat != SDL_GPU_TEXTUREFORMAT_INVALID) {
            SDL_GPUColorTargetDescription desc = {};
            desc.format = renderTargetFormat;
            desc.blend_state.enable_blend = config.enableBlending;
            if (config.enableBlending) {
                desc.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
                desc.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
                desc.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
                desc.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
                desc.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
                desc.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
            }
            desc.blend_state.color_write_mask = 0xF;
            colorTargetDescs.push_back(desc);
        }
    }

    // Set up graphics pipeline creation info
    SDL_GPUGraphicsPipelineCreateInfo createInfo = {};
    createInfo.vertex_shader = config.vertexShader->GetHandle();
    createInfo.fragment_shader = config.fragmentShader->GetHandle();
    
    createInfo.vertex_input_state.num_vertex_attributes = (uint32_t)sdlAttributes.size();
    createInfo.vertex_input_state.vertex_attributes = sdlAttributes.empty() ? nullptr : sdlAttributes.data();
    createInfo.vertex_input_state.num_vertex_buffers = sdlAttributes.empty() ? 0 : 1;
    createInfo.vertex_input_state.vertex_buffer_descriptions = sdlAttributes.empty() ? nullptr : &vertexBufferDesc;

    createInfo.primitive_type = config.primitiveType;
    createInfo.rasterizer_state.fill_mode = config.fillMode;
    createInfo.rasterizer_state.cull_mode = config.cullMode;
    createInfo.rasterizer_state.enable_depth_bias    = config.enableDepthBias;
    createInfo.rasterizer_state.depth_bias_constant_factor = config.depthBiasConstantFactor;
    createInfo.rasterizer_state.depth_bias_slope_factor    = config.depthBiasSlopeFactor;
    createInfo.rasterizer_state.depth_bias_clamp           = config.depthBiasClamp;

    createInfo.target_info.num_color_targets = (uint32_t)colorTargetDescs.size();
    createInfo.target_info.color_target_descriptions = colorTargetDescs.empty() ? nullptr : colorTargetDescs.data();

    // Set up depth stencil state
    if (config.enableDepthTest) {
        createInfo.target_info.has_depth_stencil_target = true;
        createInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        
        createInfo.depth_stencil_state.enable_depth_test = true;
        createInfo.depth_stencil_state.enable_depth_write = config.enableDepthWrite;
        createInfo.depth_stencil_state.compare_op = config.depthCompareOp;
    } else {
        createInfo.target_info.has_depth_stencil_target = false; 
        createInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_INVALID;
        
        createInfo.depth_stencil_state.enable_depth_test = false;
        createInfo.depth_stencil_state.enable_depth_write = false;
    }

    m_Pipeline = SDL_CreateGPUGraphicsPipeline(m_Device, &createInfo);
    if (!m_Pipeline) {
        spdlog::error("Failed to create Graphics Pipeline: {}", SDL_GetError());
    } else {
        spdlog::info("Graphics Pipeline created successfully.");
    }
}

GraphicsPipeline::~GraphicsPipeline() {
    if (m_Pipeline) {
        SDL_ReleaseGPUGraphicsPipeline(m_Device, m_Pipeline);
    }
}
