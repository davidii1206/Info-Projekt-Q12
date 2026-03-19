#include "Model.h"

Model::Model(SDL_GPUDevice* device, const std::vector<ModelVertex>& vertices, const std::vector<uint32_t>& indices, const std::vector<MeshSection>& sections, const std::vector<Material>& materials)
    : m_Sections(sections), m_Materials(materials)
{
    m_VertexBuffer = std::make_unique<GPUBuffer>(device, BufferUsage::Vertex, (uint32_t)(vertices.size() * sizeof(ModelVertex)));
    m_VertexBuffer->Upload(vertices.data(), (uint32_t)(vertices.size() * sizeof(ModelVertex)));

    m_IndexBuffer = std::make_unique<GPUBuffer>(device, BufferUsage::Index, (uint32_t)(indices.size() * sizeof(uint32_t)));
    m_IndexBuffer->Upload(indices.data(), (uint32_t)(indices.size() * sizeof(uint32_t)));

    // Initialize Material SSBO
    std::vector<GPUMaterial> gpuMaterials;
    for (const auto& mat : materials) {
        GPUMaterial gm;
        gm.baseColorFactor = mat.baseColorFactor;
        gm.metallicFactor = mat.metallicFactor;
        gm.roughnessFactor = mat.roughnessFactor;
        gm.baseColorTextureIndex = -1; // TODO: Map to texture array index if used
        gm.normalTextureIndex = -1;
        gpuMaterials.push_back(gm);
    }

    if (!gpuMaterials.empty()) {
        m_MaterialBuffer = std::make_unique<GPUBuffer>(device, BufferUsage::Uniform, (uint32_t)(gpuMaterials.size() * sizeof(GPUMaterial)));
        m_MaterialBuffer->Upload(gpuMaterials.data(), (uint32_t)(gpuMaterials.size() * sizeof(GPUMaterial)));
    }
}

Model::~Model() {}
