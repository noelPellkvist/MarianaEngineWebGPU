#include <Editor/GLTFLoader.hpp>
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <moved_later/tiny_gltf.h>

#include <memory>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <glm/glm.hpp>

#include <Logger.hpp>
#include <Material.hpp>
#include <AssetManager.hpp>

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

void GLTF::GLTFLoader::LoadGLTF(std::string filename, IShader& shader)
{
    std::vector<Texture> res;
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    Logger::Error("Starting to load the binary gltf");
    bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, filename);
    Logger::Error("Binary is loaded");

    if (!warn.empty()) {
      Logger::Warning(warn);
    }

    if (!err.empty()) {
      Logger::Error(err);
    }

    if (!ret) {
      Logger::Error("Failed to parse glTF");
      return;
    }

    size_t preTextures = AssetManager::LoadedTextures.size();
    size_t preMaterials = AssetManager::LoadedMaterials.size();
    size_t preMeshes = AssetManager::LoadedMeshes.size();

    Logger::Error("Start loading the gltf images");
    for (tinygltf::Image& img : model.images)
    {
        Texture newTexture;
        newTexture.CreateTexture(img.width, img.height, TextureFormat::RGBA8Unorm);
        newTexture.UploadTexture(reinterpret_cast<uint8_t*>(img.image.data()), img.image.size(), img.width, img.height);
        AssetManager::LoadedTextures.push_back(newTexture);
        res.push_back(newTexture);
    }
    Logger::Error("Textures are loaded");

    uint32_t currentMaterial = preMaterials;
    for (tinygltf::Material& mat : model.materials)
    {
        std::shared_ptr<Material<GLTFMaterialProperties>> newMat = std::make_shared<Material<GLTFMaterialProperties>>(currentMaterial);
        Texture& albedo = mat.pbrMetallicRoughness.baseColorTexture.index == -1 ? GetFlatAlbedoTexture() : AssetManager::LoadedTextures[mat.pbrMetallicRoughness.baseColorTexture.index + preTextures];
        Texture& normal = mat.normalTexture.index == -1 ? GetFlatNormalTexture() : AssetManager::LoadedTextures[mat.normalTexture.index + preTextures];
        Texture& ambient = mat.occlusionTexture.index == -1 ? GetFlatAOTexture() : AssetManager::LoadedTextures[mat.occlusionTexture.index + preTextures];
        Texture& metallicRoughness = mat.pbrMetallicRoughness.metallicRoughnessTexture.index == -1 ? GetFlatMetallicRoughnessTexture() : AssetManager::LoadedTextures[mat.pbrMetallicRoughness.metallicRoughnessTexture.index + preTextures];
        GLTFMaterialProperties props;
        props.baseColor = {1,1,1,1};
        Texture& emmisive = mat.emissiveTexture.index == -1 ? GetFlatEmissiveTexture() : AssetManager::LoadedTextures[mat.emissiveTexture.index + preTextures];
        newMat->InitMaterial(shader, {albedo, normal, ambient, metallicRoughness, emmisive});
        newMat->UpdateMaterialProperties(props);
        AssetManager::LoadedMaterials.push_back(newMat);
        currentMaterial++;
    }
    Logger::Error("Materials are loaded");

    for (tinygltf::Mesh& mesh : model.meshes)
    {
        Logger::Error("Loading mesh");
        auto newMesh = LoadEntireMesh(model, mesh, preMaterials);
        Logger::Error("Building mesh");
        newMesh->BuildMesh();
        AssetManager::LoadedMeshes.push_back(newMesh);
        Logger::Error("DONE");
    }
}