#include <Editor/GLTFLoader.hpp>
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <moved_later/tiny_gltf.h>

#include <memory>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <Logger.hpp>
#include <Material.hpp>
#include <AssetManager.hpp>
#include <Renderer.hpp>
#include <Animation.hpp>
#include <AnimationPlayer.hpp>


#pragma region BufferHelpers

// Helper: convert component to float with normalization
inline float ConvertComponent(const void* data, int componentType, bool normalized) {
    switch (componentType) {
        case TINYGLTF_COMPONENT_TYPE_BYTE: {
            int8_t v = *reinterpret_cast<const int8_t*>(data);
            return normalized ? std::max(v / 127.0f, -1.0f) : static_cast<float>(v);
        }
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
            uint8_t v = *reinterpret_cast<const uint8_t*>(data);
            return normalized ? v / 255.0f : static_cast<float>(v);
        }
        case TINYGLTF_COMPONENT_TYPE_SHORT: {
            int16_t v = *reinterpret_cast<const int16_t*>(data);
            return normalized ? std::max(v / 32767.0f, -1.0f) : static_cast<float>(v);
        }
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
            uint16_t v = *reinterpret_cast<const uint16_t*>(data);
            return normalized ? v / 65535.0f : static_cast<float>(v);
        }
        case TINYGLTF_COMPONENT_TYPE_FLOAT: {
            return *reinterpret_cast<const float*>(data);
        }
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
            uint32_t v = *reinterpret_cast<const uint32_t*>(data);
            return static_cast<float>(v); // no normalization
        }
        default:
            throw std::runtime_error("Unsupported component type");
    }
}

// Main function
template<typename T>
std::vector<T> ReadAccessor(const tinygltf::Model& model,
                            const tinygltf::Accessor& accessor)
{
    std::vector<T> result;
    result.resize(accessor.count);

    const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
    const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

    const size_t componentSize = tinygltf::GetComponentSizeInBytes(accessor.componentType);
    const size_t numComponents = tinygltf::GetNumComponentsInType(accessor.type);
    const size_t stride = accessor.ByteStride(bufferView);
    const size_t effectiveStride = stride ? stride : componentSize * numComponents;

    const uint8_t* dataPtr = buffer.data.data() + accessor.byteOffset + bufferView.byteOffset;

    for (size_t i = 0; i < accessor.count; ++i) {
        const uint8_t* elementPtr = dataPtr + i * effectiveStride;

        // Fill T with zeros in case it's bigger than accessor (e.g. glm::vec4 for vec3 data)
        T value(0.0f);

        for (size_t c = 0; c < numComponents; ++c) {
            const void* compPtr = elementPtr + c * componentSize;
            float f = ConvertComponent(compPtr, accessor.componentType, accessor.normalized);
            value[c] = f; // glm::vecN supports operator[]
        }

        result[i] = value;
    }

    return result;
}

template<>
std::vector<uint32_t> ReadAccessor<uint32_t>(const tinygltf::Model& model,
                                             const tinygltf::Accessor& accessor)
{
    std::vector<uint32_t> result(accessor.count);

    const auto& bufferView = model.bufferViews[accessor.bufferView];
    const auto& buffer     = model.buffers[bufferView.buffer];

    const size_t stride = accessor.ByteStride(bufferView);
    const size_t componentSize = tinygltf::GetComponentSizeInBytes(accessor.componentType);
    const size_t effectiveStride = stride ? stride : componentSize;
    const uint8_t* dataPtr = buffer.data.data() + accessor.byteOffset + bufferView.byteOffset;

    for (size_t i = 0; i < accessor.count; ++i) {
        const void* elementPtr = dataPtr + i * effectiveStride;
        switch (accessor.componentType) {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                result[i] = *reinterpret_cast<const uint8_t*>(elementPtr);
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                result[i] = *reinterpret_cast<const uint16_t*>(elementPtr);
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                result[i] = *reinterpret_cast<const uint32_t*>(elementPtr);
                break;
            default:
                throw std::runtime_error("Unsupported index component type");
        }
    }
    return result;
}

template<>
std::vector<float> ReadAccessor<float>(const tinygltf::Model& model,
                                       const tinygltf::Accessor& accessor)
{
    if (accessor.type != TINYGLTF_TYPE_SCALAR) {
        throw std::runtime_error("ReadAccessor<float> requires SCALAR accessor");
    }

    std::vector<float> result(accessor.count);

    const auto& bufferView = model.bufferViews[accessor.bufferView];
    const auto& buffer     = model.buffers[bufferView.buffer];

    const size_t componentSize = tinygltf::GetComponentSizeInBytes(accessor.componentType);
    const size_t stride = accessor.ByteStride(bufferView);
    const size_t effectiveStride = stride ? stride : componentSize;
    const uint8_t* dataPtr = buffer.data.data() + accessor.byteOffset + bufferView.byteOffset;

    for (size_t i = 0; i < accessor.count; ++i) {
        const void* elementPtr = dataPtr + i * effectiveStride;
        result[i] = ConvertComponent(elementPtr, accessor.componentType, accessor.normalized);
    }

    return result;
}

void LoadVertices(const tinygltf::Model& model,
                  const tinygltf::Primitive& primitive,
                  std::vector<GLTF::Vertex>& outVertices,
                  std::vector<uint32_t>& outIndices)
{
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec4> tangents;
    std::vector<glm::vec2> texcoords0;
    std::vector<glm::vec2> texcoords1;
    std::vector<glm::vec4> colors0;
    std::vector<glm::uvec4> boneIndices;
    std::vector<glm::vec4> boneWeights;

    for (const auto& attr : primitive.attributes) {
        const std::string& attrName = attr.first;
        int accessorIndex = attr.second;
        if (accessorIndex < 0 || accessorIndex >= model.accessors.size()) {
            throw std::runtime_error("Invalid accessor index for attribute: " + attrName);
        }

        const tinygltf::Accessor& accessor = model.accessors[accessorIndex];

        if (attrName == "POSITION") {
            positions = ReadAccessor<glm::vec3>(model, accessor);
        } else if (attrName == "NORMAL") {
            normals = ReadAccessor<glm::vec3>(model, accessor);
        } else if (attrName == "TANGENT") {
            tangents = ReadAccessor<glm::vec4>(model, accessor);
        } else if (attrName == "TEXCOORD_0") {
            texcoords0 = ReadAccessor<glm::vec2>(model, accessor);
        } else if (attrName == "TEXCOORD_1") {
            texcoords1 = ReadAccessor<glm::vec2>(model, accessor);
        } else if (attrName == "COLOR_0") {
            // glTF allows VEC3 or VEC4
            if (accessor.type == TINYGLTF_TYPE_VEC3)
            {
                auto tmp = ReadAccessor<glm::vec3>(model, accessor);
                colors0.resize(tmp.size());
                for (size_t i = 0; i < tmp.size(); ++i) {
                    colors0[i] = glm::vec4(tmp[i], 1.0f);
                }
            }
            else
                colors0 = ReadAccessor<glm::vec4>(model, accessor);
        } else if (attrName == "JOINTS_0") {
            boneIndices = ReadAccessor<glm::uvec4>(model, accessor);
        } else if (attrName == "WEIGHTS_0") {
            boneWeights = ReadAccessor<glm::vec4>(model, accessor);
        }
    }

    if (positions.empty()) {
        throw std::runtime_error("Primitive has no POSITION attribute (invalid glTF).");
    }

    // --- Indices ---
    outIndices.clear();
    if (primitive.indices >= 0) {
        const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
        outIndices = ReadAccessor<uint32_t>(model, indexAccessor); // must upcast smaller types
    } else {
        outIndices.resize(positions.size());
        for (size_t i = 0; i < positions.size(); ++i) {
            outIndices[i] = static_cast<uint32_t>(i);
        }
    }

    // --- Assemble vertices ---
    size_t vertexCount = positions.size();
    outVertices.resize(vertexCount);
    for (size_t i = 0; i < vertexCount; ++i) {
        GLTF::Vertex v{};
        v.position  = (i < positions.size()) ? positions[i] : glm::vec3(0.0f);
        v.normal    = (i < normals.size())   ? normals[i]   : glm::vec3(0.0f, 0.0f, 1.0f);
        v.tangent   = (i < tangents.size())  ? tangents[i]  : glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
        v.texcoord0 = (i < texcoords0.size())? texcoords0[i]: glm::vec2(0.0f);
        v.texcoord1 = (i < texcoords1.size())? texcoords1[i]: glm::vec2(0.0f);
        v.color0    = (i < colors0.size())   ? colors0[i]   : glm::vec4(1.0f);
        v.boneIndices = (i < boneIndices.size()) ? boneIndices[i] : glm::uvec4(0);
        v.boneWeights = (i < boneWeights.size()) ? boneWeights[i] : glm::vec4(0.0f);
        outVertices[i] = v;
    }
}

#pragma endregion

#pragma region TextureInitializers

Texture& GetFlatAlbedoTexture()
{
    static Texture tex;
    static bool inited = false;
    if (!inited) {
        static const uint8_t pixel[4] = { 255, 255, 255, 255 }; // white
        tex.CreateTexture(1, 1, TextureFormat::RGBA8UnormSrgb);
        tex.UploadTexture(pixel, 4, 1, 1);
        inited = true;
    }
    return tex;
}

Texture& GetFlatEmissiveTexture()
{
    static Texture tex;
    static bool inited = false;
    if (!inited) {
        static const uint8_t pixel[4] = { 0, 0, 0, 255 }; // no emission
        tex.CreateTexture(1, 1, TextureFormat::RGBA8UnormSrgb);
        tex.UploadTexture(pixel, 4, 1, 1);
        inited = true;
    }
    return tex;
}

Texture& GetFlatAOTexture()
{
    static Texture tex;
    static bool inited = false;
    if (!inited) {
        static const uint8_t pixel[4] = { 255, 255, 255, 255 };
        tex.CreateTexture(1, 1, TextureFormat::RGBA8Unorm);
        tex.UploadTexture(pixel, 4, 1, 1);
        inited = true;
    }
    return tex;
}

Texture& GetFlatNormalTexture()
{
    static Texture flat;
    static bool inited = false;
    if (!inited) {
        static const uint8_t pixel[4] = { 128, 128, 255, 255 };
        flat.CreateTexture(1, 1, TextureFormat::RGBA8Unorm);
        flat.UploadTexture(pixel, 4, 1, 1);
        inited = true;
    }
    return flat;
}

Texture& GetFlatMetallicRoughnessTexture()
{
    static Texture flat;
    static bool inited = false;
    if (!inited) {
        static const uint8_t pixel[4] = { 255, 255, 0, 255 };
        flat.CreateTexture(1, 1, TextureFormat::RGBA8Unorm);
        flat.UploadTexture(pixel, 4, 1, 1);
        inited = true;
    }
    return flat;
}

#pragma endregion

std::shared_ptr<IMesh> LoadEntireMesh(const tinygltf::Model& model, tinygltf::Mesh& rawMesh, size_t prevMaterials)
{
    std::vector<GLTF::Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Submesh> submeshes;

    for(const auto& prim : rawMesh.primitives)
    {
        std::vector<GLTF::Vertex> localVerts;
        std::vector<uint32_t>     localIdx;
        LoadVertices(model, prim, localVerts, localIdx);
        if(localVerts.empty() || localIdx.empty()) continue;

        const uint32_t baseVertex = (uint32_t)vertices.size();
        const uint32_t startIndex = (uint32_t)indices.size();
        vertices.insert(vertices.end(), localVerts.begin(), localVerts.end());

        indices.reserve(indices.size() + localIdx.size());
        for (uint32_t i : localIdx) indices.push_back(i + baseVertex);

        Submesh sm{};
        sm.startIndex    = startIndex;
        sm.indexCount    = (uint32_t)localIdx.size();
        sm.materialIndex = prim.material >= 0 ? (uint32_t)prim.material + prevMaterials : -1;
        submeshes.push_back(sm);
    }

    std::shared_ptr<IMesh> mesh = std::make_shared<Mesh<GLTF::Vertex, uint32_t>>(vertices, indices);  
    mesh->submeshes = std::move(submeshes);
    return mesh;
}

void BuildEntityFromNode(Prefab& p, Entity e, tinygltf::Model& model, int nodeIndex, uint32_t preMeshes)
{
    tinygltf::Node& n = model.nodes[nodeIndex];
    glm::vec3 translation(0.0f);
    glm::quat   rotation(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale(1.0f);

    if (n.translation.size() == 3) {
        translation = glm::vec3(
            static_cast<float>(n.translation[0]),
            static_cast<float>(n.translation[1]),
            static_cast<float>(n.translation[2])
        );
    }

    if (n.rotation.size() == 4) {
        rotation = glm::quat(
            static_cast<float>(n.rotation[3]), // w
            static_cast<float>(n.rotation[0]), // x
            static_cast<float>(n.rotation[1]), // y
            static_cast<float>(n.rotation[2])  // z
        );
    }

    if (n.scale.size() == 3) {
        scale = glm::vec3(
            static_cast<float>(n.scale[0]),
            static_cast<float>(n.scale[1]),
            static_cast<float>(n.scale[2])
        );
    }

    e.SetPosition(translation.x, translation.y, translation.z);
    e.SetRotationQuat(rotation.x, rotation.y, rotation.z, rotation.w);
    e.SetScale(scale.x, scale.y, scale.z);
    e.SetName(n.name.c_str());
    e.Add<AnimationTargetEntity>({ (uint32_t)nodeIndex });

    if (n.mesh >= 0)
        e.Add<MeshComponent>({preMeshes + n.mesh}).AddTag<ShadowCasterTag>();
        
    for (int childIndex : n.children) {
        Entity child = p.Instantiate(model.nodes[childIndex].name.c_str());
        child.SetParent(e);
        BuildEntityFromNode(p, child, model, childIndex, preMeshes);
    }
}

Animation LoadAnimation(const tinygltf::Model& model, const tinygltf::Animation& rawAnim)
{
    Animation animation(rawAnim.name.empty() ? "UnnamedAnimation" : rawAnim.name);

    for (const tinygltf::AnimationChannel& rawChannel : rawAnim.channels)
    {
        AnimationChannel channel;

        channel.targetNodeIndex = rawChannel.target_node;

        if (rawChannel.target_path == "translation")
        {
            channel.type = AnimationChannelType::Translation;
        }
        else if (rawChannel.target_path == "rotation")
        {
            channel.type = AnimationChannelType::Rotation;
        }
        else if (rawChannel.target_path == "scale")
        {
            channel.type = AnimationChannelType::Scale;
        }
        else if (rawChannel.target_path == "weights")
        {
            channel.type = AnimationChannelType::Weights;
        }

        int samplerIndex = rawChannel.sampler;
        if (samplerIndex < 0 || samplerIndex >= rawAnim.samplers.size())
        {
            throw std::runtime_error("Invalid sampler index in animation channel");
        }

        const tinygltf::AnimationSampler& rawSampler = rawAnim.samplers[samplerIndex];

        const std::string& interp = rawSampler.interpolation;
        if (interp == "LINEAR")
        {
            channel.interpolation = AnimationInterpolationType::Linear;
        }
        else if (interp == "STEP")
        {
            channel.interpolation = AnimationInterpolationType::Step;
        }
        else if (interp == "CUBICSPLINE")
        {
            channel.interpolation = AnimationInterpolationType::CubicSpline;
        }
        else
        {
            throw std::runtime_error("Unsupported interpolation type in animation sampler");
        }

        if (rawSampler.input < 0 || rawSampler.input >= model.accessors.size()) {
            throw std::runtime_error("Invalid input accessor index in animation sampler");
        }
        if (rawSampler.output < 0 || rawSampler.output >= model.accessors.size()) {
            throw std::runtime_error("Invalid output accessor index in animation sampler");
        }

        const tinygltf::Accessor& inputAccessor = model.accessors[rawSampler.input];
        const tinygltf::Accessor& outputAccessor = model.accessors[rawSampler.output];

        std::vector<float> keyframeTimes = ReadAccessor<float>(model, inputAccessor);
        const size_t keyframeCount = keyframeTimes.size();
        channel.keyFrames.resize(keyframeCount);

        for (size_t i = 0; i < keyframeCount; ++i) {
            channel.keyFrames[i].time = keyframeTimes[i];
        }

        if (channel.type == AnimationChannelType::Rotation) {
            if (outputAccessor.type != TINYGLTF_TYPE_VEC4) {
                throw std::runtime_error("Rotation output accessor must be VEC4");
            }
        } else if (channel.type == AnimationChannelType::Translation || channel.type == AnimationChannelType::Scale) {
            if (outputAccessor.type != TINYGLTF_TYPE_VEC3) {
                throw std::runtime_error("Translation/Scale output accessor must be VEC3");
            }
        } else if (channel.type == AnimationChannelType::Weights) {
            if (outputAccessor.type != TINYGLTF_TYPE_SCALAR) {
                throw std::runtime_error("Weights output accessor must be SCALAR");
            }
        }

        if (channel.interpolation == AnimationInterpolationType::Linear ||
            channel.interpolation == AnimationInterpolationType::Step)
        {
            if (channel.type == AnimationChannelType::Weights) {
                std::vector<float> values = ReadAccessor<float>(model, outputAccessor);
                if (keyframeCount == 0 || values.size() % keyframeCount != 0) {
                    throw std::runtime_error("Weights output accessor size mismatch");
                }
                const size_t weightsPerFrame = values.size() / keyframeCount;
                for (size_t i = 0; i < keyframeCount; ++i) {
                    auto begin = values.begin() + i * weightsPerFrame;
                    auto end = begin + weightsPerFrame;
                    channel.keyFrames[i].data.assign(begin, end);
                }
            } else {
                const size_t componentCount = tinygltf::GetNumComponentsInType(outputAccessor.type);
                std::vector<glm::vec4> values = ReadAccessor<glm::vec4>(model, outputAccessor);
                if (values.size() != keyframeCount) {
                    throw std::runtime_error("Animation output accessor size mismatch");
                }
                for (size_t i = 0; i < keyframeCount; ++i) {
                    channel.keyFrames[i].data.clear();
                    channel.keyFrames[i].data.reserve(componentCount);
                    for (size_t c = 0; c < componentCount; ++c) {
                        channel.keyFrames[i].data.push_back(values[i][c]);
                    }
                }
            }
        }
        else if (channel.interpolation == AnimationInterpolationType::CubicSpline)
        {
            if (channel.type == AnimationChannelType::Weights) {
                std::vector<float> values = ReadAccessor<float>(model, outputAccessor);
                if (keyframeCount == 0 || values.size() % (keyframeCount * 3) != 0) {
                    throw std::runtime_error("Weights cubic spline output accessor size mismatch");
                }
                const size_t weightsPerFrame = values.size() / (keyframeCount * 3);
                for (size_t i = 0; i < keyframeCount; ++i) {
                    const size_t base = i * weightsPerFrame * 3;
                    channel.keyFrames[i].inTangent.assign(
                        values.begin() + base,
                        values.begin() + base + weightsPerFrame
                    );
                    channel.keyFrames[i].data.assign(
                        values.begin() + base + weightsPerFrame,
                        values.begin() + base + weightsPerFrame * 2
                    );
                    channel.keyFrames[i].outTangent.assign(
                        values.begin() + base + weightsPerFrame * 2,
                        values.begin() + base + weightsPerFrame * 3
                    );
                }
            } else {
                const size_t componentCount = tinygltf::GetNumComponentsInType(outputAccessor.type);
                std::vector<glm::vec4> values = ReadAccessor<glm::vec4>(model, outputAccessor);
                if (values.size() != keyframeCount * 3) {
                    throw std::runtime_error("Animation cubic spline output accessor size mismatch");
                }
                for (size_t i = 0; i < keyframeCount; ++i) {
                    const glm::vec4 in = values[i * 3 + 0];
                    const glm::vec4 val = values[i * 3 + 1];
                    const glm::vec4 out = values[i * 3 + 2];

                    channel.keyFrames[i].inTangent.clear();
                    channel.keyFrames[i].data.clear();
                    channel.keyFrames[i].outTangent.clear();
                    channel.keyFrames[i].inTangent.reserve(componentCount);
                    channel.keyFrames[i].data.reserve(componentCount);
                    channel.keyFrames[i].outTangent.reserve(componentCount);

                    for (size_t c = 0; c < componentCount; ++c) {
                        channel.keyFrames[i].inTangent.push_back(in[c]);
                        channel.keyFrames[i].data.push_back(val[c]);
                        channel.keyFrames[i].outTangent.push_back(out[c]);
                    }
                }
            }
        }
        else
        {
            throw std::runtime_error("Unsupported interpolation type in animation sampler");
        }
        animation.AddChannel(channel);
    }
    animation.SetNodeCount(model.nodes.size()); 
    return animation.Build();
}

Prefab GLTF::GLTFLoader::LoadGLTF(std::string filename, Shader2& shader)
{
    std::vector<Texture> res;
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;
    Prefab badPrefab;

    std::string extension = std::filesystem::path(filename).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    const bool ret = extension == ".gltf"
        ? loader.LoadASCIIFromFile(&model, &err, &warn, filename)
        : loader.LoadBinaryFromFile(&model, &err, &warn, filename);

    if (!warn.empty()) {
      Logger::Warning(warn);
    }

    if (!err.empty()) {
      return std::move(badPrefab);
    }

    if (!ret) {
      Logger::Error("Failed to parse glTF");
      return std::move(badPrefab);
    }

    if (model.scenes.empty()) {
      Logger::Error("glTF has no scenes");
      return std::move(badPrefab);
    }

    if (model.scenes[0].nodes.empty()) {
      Logger::Error("glTF has no scenes");
      return std::move(badPrefab);
    }

    Prefab prefab(model.scenes[0].name.c_str());

    size_t preTextures = AssetManager::LoadedTextures.size();
    size_t preMaterials = AssetManager::LoadedMaterials.size();
    size_t preMeshes = AssetManager::LoadedMeshes.size();

    for (tinygltf::Image& img : model.images)
    {
        Texture newTexture;
        newTexture.CreateTexture(img.width, img.height, TextureFormat::RGBA8Unorm);
        newTexture.UploadTexture(reinterpret_cast<uint8_t*>(img.image.data()), img.image.size(), img.width, img.height);
        AssetManager::LoadedTextures.push_back(newTexture);
        res.push_back(newTexture);
    }

    uint32_t currentMaterial = preMaterials;
    for (tinygltf::Material& mat : model.materials)
    {
        Texture& albedo = mat.pbrMetallicRoughness.baseColorTexture.index == -1 ? GetFlatAlbedoTexture() : AssetManager::LoadedTextures[mat.pbrMetallicRoughness.baseColorTexture.index + preTextures];
        Texture& normal = mat.normalTexture.index == -1 ? GetFlatNormalTexture() : AssetManager::LoadedTextures[mat.normalTexture.index + preTextures];
        Texture& ambient = mat.occlusionTexture.index == -1 ? GetFlatAOTexture() : AssetManager::LoadedTextures[mat.occlusionTexture.index + preTextures];
        Texture& metallicRoughness = mat.pbrMetallicRoughness.metallicRoughnessTexture.index == -1 ? GetFlatMetallicRoughnessTexture() : AssetManager::LoadedTextures[mat.pbrMetallicRoughness.metallicRoughnessTexture.index + preTextures];
        GLTFMaterialProperties props;
        props.baseColor = mat.pbrMetallicRoughness.baseColorFactor.empty() ? glm::vec4(1,1,1,1) : glm::vec4(
            mat.pbrMetallicRoughness.baseColorFactor[0],
            mat.pbrMetallicRoughness.baseColorFactor[1],
            mat.pbrMetallicRoughness.baseColorFactor[2],
            mat.pbrMetallicRoughness.baseColorFactor[3]
        );
        props.metallicFactor = mat.pbrMetallicRoughness.metallicFactor;
        props.roughnessFactor = mat.pbrMetallicRoughness.roughnessFactor;
        props.emissiveFactor = {mat.emissiveFactor[0], mat.emissiveFactor[1], mat.emissiveFactor[2]};
        props.normalMapStrength = mat.normalTexture.scale;
        props.occlusionStrength = mat.occlusionTexture.strength;
        props.alphaCutoff = mat.alphaCutoff;
        AssetManager::LoadedMaterialProperties.push_back(props);
        Texture& emmisive = mat.emissiveTexture.index == -1 ? GetFlatEmissiveTexture() : AssetManager::LoadedTextures[mat.emissiveTexture.index + preTextures];
        Material2 newMat;
        newMat.InitFromShader(shader)
              .SetTexture("albedoMap", albedo)
              .SetTexture("normalMap", normal)
              .SetTexture("occlusionMap", ambient)
              .SetTexture("metallicRoughnessMap", metallicRoughness)
              .SetTexture("emissiveMap", emmisive)
              .Build();
        AssetManager::LoadedMaterials.push_back(newMat);
        currentMaterial++;
    }

    for (tinygltf::Mesh& mesh : model.meshes)
    {
        auto newMesh = LoadEntireMesh(model, mesh, preMaterials);
        newMesh->BuildMesh();
        AssetManager::LoadedMeshes.push_back(newMesh);
    }

    if (model.animations.size() > 0)
    {
        for (tinygltf::Animation& anim : model.animations)
        {
            AssetManager::LoadedAnimations.push_back(LoadAnimation(model, anim));
        }
        prefab.Add<AnimationPlayer>();
    }

    
    if(model.scenes[0].nodes.size() == 1)
        BuildEntityFromNode(prefab, prefab.Root(), model, model.scenes[0].nodes[0], (uint32_t)preMeshes);
    else
    {
        for (int i : model.scenes[0].nodes)
        {
            Entity child = prefab.Instantiate(model.nodes[i].name.c_str());
            child.SetParent(prefab.Root());
            BuildEntityFromNode(prefab, child, model, i, preMeshes);
        }
    }

    return std::move(prefab);
}
