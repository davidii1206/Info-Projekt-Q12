/**
 * @file AssetManager.cpp
 * @brief Implementation of the AssetManager for handling game assets.
 */

#define GLM_ENABLE_EXPERIMENTAL
#include "AssetManager.h"
#include <tiny_gltf.h>
#include <filesystem>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

SDL_GPUDevice* AssetManager::s_Device = nullptr;
std::unordered_map<std::string, std::shared_ptr<Texture>> AssetManager::s_Textures;
std::unordered_map<std::string, SceneData> AssetManager::s_Scenes;
std::shared_ptr<Model> AssetManager::s_FallbackModel = nullptr;
std::shared_ptr<Texture> AssetManager::s_FallbackTexture = nullptr;

void AssetManager::Init(SDL_GPUDevice* device) {
    s_Device = device;
    spdlog::info("AssetManager initialized");
}

void AssetManager::Shutdown() {
    s_Textures.clear();
    s_Scenes.clear();
    s_FallbackModel.reset();
    s_FallbackTexture.reset();
    spdlog::info("AssetManager shutdown and resources cleared");
}

std::shared_ptr<Texture> AssetManager::LoadTexture(const std::string& filePath, TextureFilter filter) {
    auto it = s_Textures.find(filePath);
    if (it != s_Textures.end()) return it->second;

    auto texture = std::make_shared<Texture>(s_Device, filePath, filter);
    if (texture->GetHandle()) {
        s_Textures[filePath] = texture;
        return texture;
    }
    return nullptr;
}

std::shared_ptr<Texture> AssetManager::LoadTexture(const std::string& name, unsigned char* pixels, uint32_t width, uint32_t height, TextureFilter filter) {
    auto it = s_Textures.find(name);
    if (it != s_Textures.end()) return it->second;

    auto texture = std::make_shared<Texture>(s_Device, pixels, width, height, filter);
    if (texture->GetHandle()) {
        s_Textures[name] = texture;
        return texture;
    }
    return nullptr;
}

/**
 * @brief Helper template to extract vertex attributes from a tinygltf model.
 * 
 * @tparam T The expected type of the attribute data.
 * @param model The tinygltf model to extract from.
 * @param accessorIndex Index of the accessor for the attribute.
 * @param stride Output parameter for the attribute's byte stride.
 * @return const T* Pointer to the attribute data, or nullptr if not found.
 */
template<typename T>
const T* GetAttributes(const tinygltf::Model& model, int accessorIndex, size_t& stride) {
    if (accessorIndex < 0) return nullptr;
    const tinygltf::Accessor& acc = model.accessors[accessorIndex];
    const tinygltf::BufferView& bv = model.bufferViews[acc.bufferView];
    stride = acc.ByteStride(bv);
    if (stride == 0) {
        stride = tinygltf::GetComponentSizeInBytes(acc.componentType) * tinygltf::GetNumComponentsInType(acc.type);
    }
    return reinterpret_cast<const T*>(&(model.buffers[bv.buffer].data[acc.byteOffset + bv.byteOffset]));
}

/**
 * @struct LoaderContext
 * @brief Context structure for passing around data during the GLTF loading process.
 */
struct LoaderContext {
    std::vector<ModelVertex>& vertices; ///< Reference to the global vertex list.
    std::vector<uint32_t>& indices; ///< Reference to the global index list.
    std::vector<MeshSection>& sections; ///< Reference to the global mesh section list.
    std::vector<Light>& lights; ///< Reference to the list of lights found in the scene.
    std::vector<MeshInstance>& meshInstances; ///< Reference to the list of mesh instances.
    const tinygltf::Model& gltfModel; ///< Reference to the loaded tinygltf model structure.
    const std::string& filePath; ///< Path to the source GLTF file.
    const std::string& baseDir; ///< Directory containing the GLTF file for relative paths.
};

/**
 * @brief Recursively processes nodes in a GLTF scene graph.
 * 
 * Extracts meshes, transforms, and lights from nodes and populates the LoaderContext.
 * 
 * @param ctx The current loader context.
 * @param nodeIndex Index of the node to process.
 * @param parentTransform The transformation matrix of the parent node.
 */
void ProcessNode(LoaderContext& ctx, int nodeIndex, const glm::mat4& parentTransform) {
    if (nodeIndex < 0) return;
    const auto& node = ctx.gltfModel.nodes[nodeIndex];

    glm::mat4 localTransform = glm::mat4(1.0f);
    if (node.matrix.size() == 16) {
        float mat[16];
        for (int i = 0; i < 16; i++) mat[i] = (float)node.matrix[i];
        localTransform = glm::make_mat4(mat);
    } else {
        if (node.translation.size() == 3) {
            localTransform = glm::translate(localTransform, glm::vec3((float)node.translation[0], (float)node.translation[1], (float)node.translation[2]));
        }
        if (node.rotation.size() == 4) {
            localTransform = localTransform * glm::toMat4(glm::quat((float)node.rotation[3], (float)node.rotation[0], (float)node.rotation[1], (float)node.rotation[2]));
        }
        if (node.scale.size() == 3) {
            localTransform = glm::scale(localTransform, glm::vec3((float)node.scale[0], (float)node.scale[1], (float)node.scale[2]));
        }
    }

    glm::mat4 globalTransform = parentTransform * localTransform;

    /// Handle KHR_lights_punctual extension.
    if (node.extensions.count("KHR_lights_punctual")) {
        spdlog::debug("Node {}: Found KHR_lights_punctual extension", nodeIndex);
        const auto& ext = node.extensions.at("KHR_lights_punctual");
        if (ext.IsObject() && ext.Has("light")) {
            int lightIdx = ext.Get("light").Get<int>();
            spdlog::debug("Node {}: Extension references light index {}", nodeIndex, lightIdx);
            const tinygltf::Light* gltfLight = (lightIdx >= 0 && lightIdx < (int)ctx.gltfModel.lights.size()) ? &ctx.gltfModel.lights[lightIdx] : nullptr;

            if (gltfLight) {
                spdlog::info("Node {}: Successfully found light '{}' of type {}", nodeIndex, gltfLight->name, gltfLight->type);
                Light light;
                glm::vec4 worldPos = globalTransform * glm::vec4(0, 0, 0, 1);
                
                glm::vec4 worldDir = globalTransform * glm::vec4(0, 0, -1, 0);
                glm::vec3 direction = glm::normalize(glm::vec3(worldDir));

                glm::vec3 color = (gltfLight->color.size() == 3) ? glm::vec3((float)gltfLight->color[0], (float)gltfLight->color[1], (float)gltfLight->color[2]) : glm::vec3(1.0f);
                float intensity = static_cast<float>(gltfLight->intensity);
                
                /** 
                 * Proper intensity scaling:
                 * KHR_lights_punctual:
                 * - Directional: Lux (lm/m^2)
                 * - Point/Spot: Candela (lm/sr) or Watts (roughly)
                 * Most glTF exporters use very high values. We normalize them to a 0-10 scale for our simple shader.
                 */
                if (gltfLight->type == "directional") {
                    intensity *= 0.0001f; // Normalize Sun-like Lux (100k) to ~10.0
                } else {
                    intensity *= 0.01f;   // Normalize typical Candela (500-1000) to ~5.0-10.0
                }

                float range = (float)gltfLight->range;
                if (range <= 0.0f) range = 1000.0f; // Default large range if not specified
                uint32_t type = (uint32_t)LightType::Point;
                
                if (gltfLight->type == "directional") type = (uint32_t)LightType::Directional;
                else if (gltfLight->type == "point") type = (uint32_t)LightType::Point;
                else if (gltfLight->type == "spot") type = (uint32_t)LightType::Spot;
                
                light.position_type = glm::vec4(glm::vec3(worldPos), (float)type);
                light.direction_range = glm::vec4(direction, range);
                light.color_intensity = glm::vec4(color, intensity);

                spdlog::info("  - Position:  ({}, {}, {})", worldPos.x, worldPos.y, worldPos.z);
                spdlog::info("  - Direction: ({}, {}, {})", direction.x, direction.y, direction.z);
                spdlog::info("  - Color:     ({}, {}, {})", color.r, color.g, color.b);
                spdlog::info("  - Intensity: {}", intensity);
                spdlog::info("  - Range:     {}", range);
                spdlog::info("  - Type Enum: {}", type);

                ctx.lights.push_back(light);
            }
        }
    }

    if (node.mesh >= 0) {
        const auto& mesh = ctx.gltfModel.meshes[node.mesh];
        MeshInstance instance;
        instance.firstSection = (uint32_t)ctx.sections.size();
        instance.sectionCount = (uint32_t)mesh.primitives.size();
        instance.transform = globalTransform;
        ctx.meshInstances.push_back(instance);

        for (const auto& primitive : mesh.primitives) {
            MeshSection section;
            section.firstIndex = (uint32_t)ctx.indices.size();
            section.materialIndex = primitive.material >= 0 ? (uint32_t)primitive.material : 0;

            const uint8_t* posPtr = nullptr;
            const uint8_t* normPtr = nullptr;
            const uint8_t* uvPtr = nullptr;
            const uint8_t* colorPtr = nullptr;
            size_t posStride = 0, normStride = 0, uvStride = 0, colorStride = 0;
            int colorCompType = 0, colorType = 0;

            if (primitive.attributes.count("POSITION")) posPtr = GetAttributes<uint8_t>(ctx.gltfModel, primitive.attributes.at("POSITION"), posStride);
            if (primitive.attributes.count("NORMAL")) normPtr = GetAttributes<uint8_t>(ctx.gltfModel, primitive.attributes.at("NORMAL"), normStride);
            if (primitive.attributes.count("TEXCOORD_0")) uvPtr = GetAttributes<uint8_t>(ctx.gltfModel, primitive.attributes.at("TEXCOORD_0"), uvStride);
            if (primitive.attributes.count("COLOR_0")) {
                const auto& acc = ctx.gltfModel.accessors[primitive.attributes.at("COLOR_0")];
                colorPtr = GetAttributes<uint8_t>(ctx.gltfModel, primitive.attributes.at("COLOR_0"), colorStride);
                colorCompType = acc.componentType;
                colorType = acc.type;
            }

            size_t vertexCount = ctx.gltfModel.accessors[primitive.attributes.at("POSITION")].count;
            uint32_t vertexStart = (uint32_t)ctx.vertices.size();

            for (size_t i = 0; i < vertexCount; i++) {
                ModelVertex v;
                if (posPtr) v.position = glm::make_vec3(reinterpret_cast<const float*>(posPtr + i * posStride));
                else v.position = glm::vec3(0.0f);

                if (normPtr) v.normal = glm::make_vec3(reinterpret_cast<const float*>(normPtr + i * normStride));
                else v.normal = glm::vec3(0.0f, 1.0f, 0.0f);

                if (uvPtr) v.texCoords = glm::make_vec2(reinterpret_cast<const float*>(uvPtr + i * uvStride));
                else v.texCoords = glm::vec2(0.0f);

                v.color = glm::vec4(1.0f);
                if (colorPtr) {
                    const uint8_t* cPtr = colorPtr + i * colorStride;
                    float factor = (colorCompType == TINYGLTF_COMPONENT_TYPE_FLOAT) ? 1.0f : (colorCompType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT ? 1.0f/65535.0f : 1.0f/255.0f);
                    if (colorCompType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
                        const float* f = reinterpret_cast<const float*>(cPtr);
                        v.color = (colorType == TINYGLTF_TYPE_VEC3) ? glm::vec4(f[0], f[1], f[2], 1.0f) : glm::make_vec4(f);
                    } else if (colorCompType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                        const uint16_t* u = reinterpret_cast<const uint16_t*>(cPtr);
                        v.color = (colorType == TINYGLTF_TYPE_VEC3) ? glm::vec4(u[0]*factor, u[1]*factor, u[2]*factor, 1.0f) : glm::vec4(u[0]*factor, u[1]*factor, u[2]*factor, u[3]*factor);
                    } else {
                        const uint8_t* u = cPtr;
                        v.color = (colorType == TINYGLTF_TYPE_VEC3) ? glm::vec4(u[0]*factor, u[1]*factor, u[2]*factor, 1.0f) : glm::vec4(u[0]*factor, u[1]*factor, u[2]*factor, u[3]*factor);
                    }
                }
                ctx.vertices.push_back(v);
            }

            if (primitive.indices >= 0) {
                const auto& acc = ctx.gltfModel.accessors[primitive.indices];
                size_t indexStride = 0;
                const auto* buf = GetAttributes<uint8_t>(ctx.gltfModel, primitive.indices, indexStride);
                section.indexCount = (uint32_t)acc.count;
                for (size_t i = 0; i < acc.count; i++) {
                    const uint8_t* iPtr = buf + i * indexStride;
                    if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) ctx.indices.push_back(*reinterpret_cast<const uint32_t*>(iPtr) + vertexStart);
                    else if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) ctx.indices.push_back(*reinterpret_cast<const uint16_t*>(iPtr) + vertexStart);
                    else if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) ctx.indices.push_back(*iPtr + vertexStart);
                }
            } else {
                section.indexCount = (uint32_t)vertexCount;
                for (uint32_t i = 0; i < (uint32_t)vertexCount; i++) ctx.indices.push_back(i + vertexStart);
            }
            ctx.sections.push_back(section);
        }
    }

    for (int childIndex : node.children) ProcessNode(ctx, childIndex, globalTransform);
}

std::shared_ptr<Model> AssetManager::LoadModel(const std::string& filePath) {
    return LoadGLTF(filePath).model;
}


const SceneData& AssetManager::LoadGLTF(const std::string& filePath) {
    auto it = s_Scenes.find(filePath);
    if (it != s_Scenes.end()) return it->second;

    tinygltf::Model gltfModel;
    tinygltf::TinyGLTF loader;
    std::string err, warn;
    bool ret = false;
    std::string ext = std::filesystem::path(filePath).extension().string();
    if (ext == ".gltf") ret = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, filePath);
    else if (ext == ".glb") ret = loader.LoadBinaryFromFile(&gltfModel, &err, &warn, filePath);

    if (!ret) { spdlog::error("Failed to load GLTF: {}", filePath); static const SceneData kEmpty; return kEmpty; }

    spdlog::info("GLTF Loaded: {}. Used Extensions:", filePath);
    for (const auto& extName : gltfModel.extensionsUsed) {
        spdlog::info("  - {}", extName);
    }
    spdlog::info("Model has {} lights in the main lights vector", gltfModel.lights.size());
    if (gltfModel.extensions.count("KHR_lights_punctual")) {
        const auto& ext = gltfModel.extensions.at("KHR_lights_punctual");
        if (ext.IsObject() && ext.Has("lights")) {
            spdlog::info("KHR_lights_punctual found in root extensions with {} lights", ext.Get("lights").ArrayLen());
        }
    }

    std::vector<ModelVertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<MeshSection> sections;
    std::vector<Material> materials;
    std::vector<Light> lights;
    std::vector<MeshInstance> meshInstances;

    std::string baseDir = std::filesystem::path(filePath).parent_path().string();
    for (size_t i = 0; i < gltfModel.materials.size(); i++) {
        const auto& gltfMat = gltfModel.materials[i];
        Material mat;
        mat.name = gltfMat.name.empty() ? "Material_" + std::to_string(i) : gltfMat.name;
        mat.baseColorFactor = glm::vec4(
            static_cast<float>(gltfMat.pbrMetallicRoughness.baseColorFactor[0]),
            static_cast<float>(gltfMat.pbrMetallicRoughness.baseColorFactor[1]),
            static_cast<float>(gltfMat.pbrMetallicRoughness.baseColorFactor[2]),
            static_cast<float>(gltfMat.pbrMetallicRoughness.baseColorFactor[3])
        );
        mat.metallicFactor = (float)gltfMat.pbrMetallicRoughness.metallicFactor;
        mat.roughnessFactor = (float)gltfMat.pbrMetallicRoughness.roughnessFactor;

        if (gltfMat.pbrMetallicRoughness.baseColorTexture.index >= 0) {
            const auto& gltfTex = gltfModel.textures[gltfMat.pbrMetallicRoughness.baseColorTexture.index];
            const auto& gltfImg = gltfModel.images[gltfTex.source];
            if (!gltfImg.uri.empty()) mat.baseColorTexture = LoadTexture(baseDir + "/" + gltfImg.uri);
            else if (!gltfImg.image.empty()) {
                std::string texName = filePath + "_img_" + std::to_string(gltfTex.source);
                mat.baseColorTexture = LoadTexture(texName, (unsigned char*)gltfImg.image.data(), gltfImg.width, gltfImg.height);
            }
        }
        materials.push_back(mat);
    }
    if (materials.empty()) materials.push_back(Material{"Default"});

    LoaderContext ctx = { vertices, indices, sections, lights, meshInstances, gltfModel, filePath, baseDir };
    const tinygltf::Scene& scene = gltfModel.scenes[gltfModel.defaultScene >= 0 ? gltfModel.defaultScene : 0];
    for (int nodeIndex : scene.nodes) ProcessNode(ctx, nodeIndex, glm::mat4(1.0f));

    auto model = std::make_shared<Model>(s_Device, vertices, indices, sections, materials);
    SceneData sceneData = { model, lights, meshInstances, std::move(vertices), std::move(indices) };
    auto [ins, _] = s_Scenes.emplace(filePath, std::move(sceneData));
    spdlog::info("Scene loaded successfully: {} ({} vertices, {} indices, {} lights, {} meshes)", filePath, (uint32_t)ins->second.cpuVertices.size(), (uint32_t)ins->second.cpuIndices.size(), (uint32_t)ins->second.lights.size(), (uint32_t)ins->second.meshInstances.size());
    return ins->second;
}

const SceneData& AssetManager::RegisterProceduralScene(
    const std::string& key,
    std::vector<ModelVertex> vertices,
    std::vector<uint32_t> indices,
    const std::vector<MeshSection>& sections,
    const std::vector<Material>& materials)
{
    auto model = std::make_shared<Model>(s_Device, vertices, indices, sections, materials);
    std::vector<MeshInstance> meshInstances = {
        { 0, (uint32_t)sections.size(), glm::mat4(1.0f) }
    };
    SceneData sceneData = { model, {}, meshInstances, std::move(vertices), std::move(indices) };
    auto& ins = s_Scenes[key] = std::move(sceneData);
    spdlog::info("AssetManager: registered procedural scene '{}' ({} vertices, {} indices, {} sections)",
                  key, ins.cpuVertices.size(), ins.cpuIndices.size(), sections.size());
    return ins;
}

std::shared_ptr<Model> AssetManager::GetFallbackModel() {
    if (s_FallbackModel) return s_FallbackModel;
    std::vector<ModelVertex> vertices = {
        {{-0.5f, -0.5f,  0.5f}, {0,0,1}, {0,0}, {1,1,1,1}}, {{ 0.5f, -0.5f,  0.5f}, {0,0,1}, {1,0}, {1,1,1,1}},
        {{ 0.5f,  0.5f,  0.5f}, {0,0,1}, {1,1}, {1,1,1,1}}, {{-0.5f,  0.5f,  0.5f}, {0,0,1}, {0,1}, {1,1,1,1}},
        {{-0.5f, -0.5f, -0.5f}, {0,0,-1}, {1,0}, {1,1,1,1}}, {{-0.5f,  0.5f, -0.5f}, {0,0,-1}, {1,1}, {1,1,1,1}},
        {{ 0.5f,  0.5f, -0.5f}, {0,0,-1}, {0,1}, {1,1,1,1}}, {{ 0.5f, -0.5f, -0.5f}, {0,0,-1}, {0,0}, {1,1,1,1}},
        {{-0.5f,  0.5f, -0.5f}, {0,1,0}, {0,1}, {1,1,1,1}}, {{-0.5f,  0.5f,  0.5f}, {0,1,0}, {0,0}, {1,1,1,1}},
        {{ 0.5f,  0.5f,  0.5f}, {0,1,0}, {1,0}, {1,1,1,1}}, {{ 0.5f,  0.5f, -0.5f}, {0,1,0}, {1,1}, {1,1,1,1}},
        {{-0.5f, -0.5f, -0.5f}, {0,-1,0}, {1,1}, {1,1,1,1}}, {{ 0.5f, -0.5f, -0.5f}, {0,-1,0}, {0,1}, {1,1,1,1}},
        {{ 0.5f, -0.5f,  0.5f}, {0,-1,0}, {0,0}, {1,1,1,1}}, {{-0.5f, -0.5f,  0.5f}, {0,-1,0}, {1,0}, {1,1,1,1}},
        {{ 0.5f, -0.5f, -0.5f}, {1,0,0}, {1,0}, {1,1,1,1}}, {{ 0.5f,  0.5f, -0.5f}, {1,0,0}, {1,1}, {1,1,1,1}},
        {{ 0.5f,  0.5f,  0.5f}, {1,0,0}, {0,1}, {1,1,1,1}}, {{ 0.5f, -0.5f,  0.5f}, {1,0,0}, {0,0}, {1,1,1,1}},
        {{-0.5f, -0.5f, -0.5f}, {-1,0,0}, {0,0}, {1,1,1,1}}, {{-0.5f, -0.5f,  0.5f}, {-1,0,0}, {1,0}, {1,1,1,1}},
        {{-0.5f,  0.5f,  0.5f}, {-1,0,0}, {1,1}, {1,1,1,1}}, {{-0.5f,  0.5f, -0.5f}, {-1,0,0}, {0,1}, {1,1,1,1}}
    };
    std::vector<uint32_t> indices = {
        0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4,
        8, 9, 10, 10, 11, 8, 12, 13, 14, 14, 15, 12,
        16, 17, 18, 18, 19, 16, 20, 21, 22, 22, 23, 20
    };
    std::vector<MeshSection> sections = {{0, 36, 0}};
    std::vector<Material> materials = {{"Default"}};
    materials[0].baseColorFactor = {1.0f, 0.0f, 1.0f, 1.0f};
    s_FallbackModel = std::make_shared<Model>(s_Device, vertices, indices, sections, materials);
    return s_FallbackModel;
}

std::shared_ptr<Texture> AssetManager::GetFallbackTexture() {
    if (s_FallbackTexture) return s_FallbackTexture;
    unsigned char whitePixels[] = { 255, 255, 255, 255 };
    s_FallbackTexture = std::make_shared<Texture>(s_Device, whitePixels, 1, 1, TextureFilter::Nearest);
    return s_FallbackTexture;
}
