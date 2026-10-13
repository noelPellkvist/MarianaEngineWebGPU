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
#include <string>
#include <vector>
#include <cstring>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <Logger.hpp>
#include <Material.hpp>
#include <MaterialAsset.hpp>
#include <BuiltinAssets.hpp>
#include <Renderer.hpp>
#include <Animation.hpp>
#include <AnimationPlayer.hpp>
#include <Skin.hpp>


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

// Calls `read(index, element)` for every sparse substitution of `accessor`. Sparse values are
// tightly packed elements of the accessor's own type.
template<typename ReadElement>
void ForEachSparseValue(const tinygltf::Model& model, const tinygltf::Accessor& accessor, size_t elementSize, ReadElement read)
{
    const auto& sparse = accessor.sparse;
    if (!sparse.isSparse || sparse.count <= 0)
        return;
    if (sparse.indices.bufferView < 0 || sparse.indices.bufferView >= (int)model.bufferViews.size() ||
        sparse.values.bufferView < 0 || sparse.values.bufferView >= (int)model.bufferViews.size())
        throw std::runtime_error("Invalid sparse accessor");

    const tinygltf::BufferView& indexView = model.bufferViews[sparse.indices.bufferView];
    const tinygltf::BufferView& valueView = model.bufferViews[sparse.values.bufferView];
    const uint8_t* indices = model.buffers[indexView.buffer].data.data() + indexView.byteOffset + sparse.indices.byteOffset;
    const uint8_t* values = model.buffers[valueView.buffer].data.data() + valueView.byteOffset + sparse.values.byteOffset;
    const size_t indexSize = tinygltf::GetComponentSizeInBytes(sparse.indices.componentType);

    for (int i = 0; i < sparse.count; ++i)
    {
        uint32_t index = 0;
        if (indexSize == 1) {
            index = indices[i];
        } else if (indexSize == 2) {
            uint16_t value;
            std::memcpy(&value, indices + i * 2, sizeof(value));
            index = value;
        } else {
            std::memcpy(&index, indices + i * 4, sizeof(index));
        }
        if (index < accessor.count)
            read(index, values + static_cast<size_t>(i) * elementSize);
    }
}

// Start of an accessor's data and its element stride, or nullptr when it has no buffer view: then
// it is all zeros (plus any sparse values).
const uint8_t* AccessorData(const tinygltf::Model& model, const tinygltf::Accessor& accessor, size_t elementSize, size_t& stride)
{
    stride = elementSize;
    if (accessor.bufferView < 0 || accessor.bufferView >= (int)model.bufferViews.size())
        return nullptr;

    const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
    const int byteStride = accessor.ByteStride(bufferView);
    if (byteStride > 0)
        stride = static_cast<size_t>(byteStride);
    return model.buffers[bufferView.buffer].data.data() + bufferView.byteOffset + accessor.byteOffset;
}

// Main function
template<typename T>
std::vector<T> ReadAccessor(const tinygltf::Model& model,
                            const tinygltf::Accessor& accessor)
{
    std::vector<T> result(accessor.count, T(0.0f));

    const size_t componentSize = tinygltf::GetComponentSizeInBytes(accessor.componentType);
    const size_t numComponents = tinygltf::GetNumComponentsInType(accessor.type);
    const size_t elementSize = componentSize * numComponents;

    // Fills T with zeros in case it's bigger than the accessor's type (e.g. glm::vec4 for vec3 data).
    auto convert = [&](const uint8_t* element) {
        T value(0.0f);
        for (size_t c = 0; c < numComponents; ++c)
            value[c] = ConvertComponent(element + c * componentSize, accessor.componentType, accessor.normalized); // glm::vecN supports operator[]
        return value;
    };

    size_t stride = 0;
    if (const uint8_t* dataPtr = AccessorData(model, accessor, elementSize, stride))
    {
        // Float data is the common case: copy it as-is instead of converting component by component.
        if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && numComponents * sizeof(float) <= sizeof(T)) {
            if (stride == sizeof(T)) {
                std::memcpy(result.data(), dataPtr, accessor.count * sizeof(T));
            } else {
                for (size_t i = 0; i < accessor.count; ++i)
                    std::memcpy(&result[i], dataPtr + i * stride, numComponents * sizeof(float));
            }
        } else {
            for (size_t i = 0; i < accessor.count; ++i)
                result[i] = convert(dataPtr + i * stride);
        }
    }

    ForEachSparseValue(model, accessor, elementSize, [&](uint32_t index, const uint8_t* element) { result[index] = convert(element); });
    return result;
}

template<>
std::vector<uint32_t> ReadAccessor<uint32_t>(const tinygltf::Model& model,
                                             const tinygltf::Accessor& accessor)
{
    std::vector<uint32_t> result(accessor.count, 0u);

    const size_t componentSize = tinygltf::GetComponentSizeInBytes(accessor.componentType);
    auto readIndex = [&](const uint8_t* element) -> uint32_t {
        switch (accessor.componentType) {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                return *element;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                uint16_t value;
                std::memcpy(&value, element, sizeof(value));
                return value;
            }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
                uint32_t value;
                std::memcpy(&value, element, sizeof(value));
                return value;
            }
            default:
                throw std::runtime_error("Unsupported index component type");
        }
    };

    size_t effectiveStride = 0;
    if (const uint8_t* dataPtr = AccessorData(model, accessor, componentSize, effectiveStride))
    {
        // One loop per index type, so the type isn't re-checked for every index.
        auto copyIndices = [&](auto type) {
            using IndexT = decltype(type);
            for (size_t i = 0; i < accessor.count; ++i) {
                IndexT value;
                std::memcpy(&value, dataPtr + i * effectiveStride, sizeof(IndexT));
                result[i] = value;
            }
        };

        switch (accessor.componentType) {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                copyIndices(uint8_t{});
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                copyIndices(uint16_t{});
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                if (effectiveStride == sizeof(uint32_t))
                    std::memcpy(result.data(), dataPtr, accessor.count * sizeof(uint32_t));
                else
                    copyIndices(uint32_t{});
                break;
            default:
                throw std::runtime_error("Unsupported index component type");
        }
    }

    ForEachSparseValue(model, accessor, componentSize, [&](uint32_t index, const uint8_t* element) { result[index] = readIndex(element); });
    return result;
}

template<>
std::vector<float> ReadAccessor<float>(const tinygltf::Model& model,
                                       const tinygltf::Accessor& accessor)
{
    if (accessor.type != TINYGLTF_TYPE_SCALAR) {
        throw std::runtime_error("ReadAccessor<float> requires SCALAR accessor");
    }

    std::vector<float> result(accessor.count, 0.0f);

    const size_t componentSize = tinygltf::GetComponentSizeInBytes(accessor.componentType);
    size_t effectiveStride = 0;
    if (const uint8_t* dataPtr = AccessorData(model, accessor, componentSize, effectiveStride))
    {
        for (size_t i = 0; i < accessor.count; ++i)
            result[i] = ConvertComponent(dataPtr + i * effectiveStride, accessor.componentType, accessor.normalized);
    }

    ForEachSparseValue(model, accessor, componentSize, [&](uint32_t index, const uint8_t* element) {
        result[index] = ConvertComponent(element, accessor.componentType, accessor.normalized);
    });
    return result;
}

// MAT4 floats (inverse bind matrices), column-major like glm.
template<>
std::vector<glm::mat4> ReadAccessor<glm::mat4>(const tinygltf::Model& model,
                                               const tinygltf::Accessor& accessor)
{
    if (accessor.type != TINYGLTF_TYPE_MAT4 || accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) {
        throw std::runtime_error("ReadAccessor<mat4> requires a MAT4 float accessor");
    }

    std::vector<glm::mat4> result(accessor.count, glm::mat4(0.0f));

    size_t stride = 0;
    if (const uint8_t* dataPtr = AccessorData(model, accessor, sizeof(glm::mat4), stride))
    {
        for (size_t i = 0; i < accessor.count; ++i)
            std::memcpy(&result[i], dataPtr + i * stride, sizeof(glm::mat4));
    }

    ForEachSparseValue(model, accessor, sizeof(glm::mat4), [&](uint32_t index, const uint8_t* element) {
        std::memcpy(&result[index], element, sizeof(glm::mat4));
    });
    return result;
}

// Tangents are optional in glTF, but normal maps need them. Each triangle's direction of
// increasing u (and v) is summed on its vertices (Lengyel's method), then made perpendicular to the
// normal. w picks the bitangent side so it points up the texture (decreasing v), as glTF expects.
void GenerateTangents(std::vector<GLTF::Vertex>& vertices, const std::vector<uint32_t>& indices)
{
    std::vector<glm::vec3> tangents(vertices.size(), glm::vec3(0.0f));
    std::vector<glm::vec3> bitangents(vertices.size(), glm::vec3(0.0f));
    for (size_t i = 0; i + 2 < indices.size(); i += 3)
    {
        const uint32_t i0 = indices[i], i1 = indices[i + 1], i2 = indices[i + 2];
        if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size())
            continue;

        const glm::vec3 e1 = vertices[i1].position - vertices[i0].position;
        const glm::vec3 e2 = vertices[i2].position - vertices[i0].position;
        const glm::vec2 d1 = vertices[i1].texcoord0 - vertices[i0].texcoord0;
        const glm::vec2 d2 = vertices[i2].texcoord0 - vertices[i0].texcoord0;
        const float det = d1.x * d2.y - d2.x * d1.y;
        if (std::abs(det) < 1e-12f)
            continue;

        const glm::vec3 t = (e1 * d2.y - e2 * d1.y) / det;
        const glm::vec3 b = (e2 * d1.x - e1 * d2.x) / det;
        for (const uint32_t index : { i0, i1, i2 })
        {
            tangents[index] += t;
            bitangents[index] += b;
        }
    }

    for (size_t i = 0; i < vertices.size(); ++i)
    {
        const glm::vec3 n = vertices[i].normal;
        glm::vec3 t = tangents[i] - n * glm::dot(n, tangents[i]);
        // No usable UVs around this vertex: any direction along the surface.
        if (glm::dot(t, t) < 1e-12f)
            t = glm::cross(n, std::abs(n.y) < 0.99f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f));
        t = glm::normalize(t);
        const float w = glm::dot(glm::cross(n, t), bitangents[i]) < 0.0f ? 1.0f : -1.0f;
        vertices[i].tangent = glm::vec4(t, w);
    }
}

// Turns glTF strips, fans and loops into plain lists (the only topologies the pipelines draw),
// keeping every triangle's winding.
PrimitiveTopology ToListTopology(int mode, std::vector<uint32_t>& indices)
{
    std::vector<uint32_t> list;
    switch (mode)
    {
        case TINYGLTF_MODE_POINTS:
            return PrimitiveTopology::Points;
        case TINYGLTF_MODE_LINE:
            indices.resize(indices.size() & ~size_t(1));
            return PrimitiveTopology::Lines;
        case TINYGLTF_MODE_LINE_LOOP:
        case TINYGLTF_MODE_LINE_STRIP:
            for (size_t i = 0; i + 1 < indices.size(); ++i) {
                list.push_back(indices[i]);
                list.push_back(indices[i + 1]);
            }
            if (mode == TINYGLTF_MODE_LINE_LOOP && indices.size() > 2) {
                list.push_back(indices.back());
                list.push_back(indices.front());
            }
            indices.swap(list);
            return PrimitiveTopology::Lines;
        case TINYGLTF_MODE_TRIANGLE_STRIP:
            // Every other triangle of a strip is wound the other way, so it is flipped back.
            for (size_t i = 0; i + 2 < indices.size(); ++i) {
                const bool odd = (i % 2) == 1;
                list.push_back(indices[i]);
                list.push_back(indices[i + (odd ? 2 : 1)]);
                list.push_back(indices[i + (odd ? 1 : 2)]);
            }
            indices.swap(list);
            return PrimitiveTopology::Triangles;
        case TINYGLTF_MODE_TRIANGLE_FAN:
            for (size_t i = 0; i + 2 < indices.size(); ++i) {
                list.push_back(indices[i + 1]);
                list.push_back(indices[i + 2]);
                list.push_back(indices[0]);
            }
            indices.swap(list);
            return PrimitiveTopology::Triangles;
        default:
            indices.resize(indices.size() - indices.size() % 3);
            return PrimitiveTopology::Triangles;
    }
}

void LoadVertices(const tinygltf::Model& model,
                  const tinygltf::Primitive& primitive,
                  std::vector<GLTF::Vertex>& outVertices,
                  std::vector<uint32_t>& outIndices,
                  PrimitiveTopology& outTopology)
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
        v.normal    = (i < normals.size())   ? normals[i]   : glm::vec3(0.0f);
        v.tangent   = (i < tangents.size())  ? tangents[i]  : glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
        v.texcoord0 = (i < texcoords0.size())? texcoords0[i]: glm::vec2(0.0f);
        v.texcoord1 = (i < texcoords1.size())? texcoords1[i]: glm::vec2(0.0f);
        v.color0    = (i < colors0.size())   ? colors0[i]   : glm::vec4(1.0f);
        v.boneIndices = (i < boneIndices.size()) ? boneIndices[i] : glm::uvec4(0);
        v.boneWeights = (i < boneWeights.size()) ? boneWeights[i] : glm::vec4(0.0f);
        // Exporters often leave weights not quite summing to 1, which shrinks or swells the skin.
        const float weightSum = v.boneWeights.x + v.boneWeights.y + v.boneWeights.z + v.boneWeights.w;
        if (weightSum > 0.0f) v.boneWeights /= weightSum;
        outVertices[i] = v;
    }

    outTopology = ToListTopology(primitive.mode, outIndices);

    // Without normals glTF asks for flat shading: every triangle gets its own three vertices with
    // the face's normal, and any tangents given are ignored. Points and lines keep a zero normal,
    // which the PBR shader draws unlit.
    if (normals.empty() && outTopology == PrimitiveTopology::Triangles)
    {
        std::vector<GLTF::Vertex> flat;
        flat.reserve(outIndices.size());
        for (size_t i = 0; i + 2 < outIndices.size(); i += 3)
        {
            if (outIndices[i] >= vertexCount || outIndices[i + 1] >= vertexCount || outIndices[i + 2] >= vertexCount)
                continue;
            GLTF::Vertex a = outVertices[outIndices[i]];
            GLTF::Vertex b = outVertices[outIndices[i + 1]];
            GLTF::Vertex c = outVertices[outIndices[i + 2]];
            const glm::vec3 n = glm::cross(b.position - a.position, c.position - a.position);
            a.normal = b.normal = c.normal = glm::dot(n, n) > 1e-20f ? glm::normalize(n) : glm::vec3(0.0f, 0.0f, 1.0f);
            flat.push_back(a);
            flat.push_back(b);
            flat.push_back(c);
        }
        outVertices.swap(flat);
        outIndices.resize(outVertices.size());
        for (size_t i = 0; i < outIndices.size(); ++i)
            outIndices[i] = static_cast<uint32_t>(i);
        tangents.clear();
    }

    if (tangents.empty() && outTopology == PrimitiveTopology::Triangles)
        GenerateTangents(outVertices, outIndices);
}

#pragma endregion

#pragma region Images

// Expands a decoded glTF image (1-4 channels, 8 or 16 bits per channel) to tightly packed RGBA8.
bool ToRGBA8(const tinygltf::Image& image, std::vector<uint8_t>& out)
{
    const int channels = image.component;
    const int bytesPerChannel = image.bits == 16 ? 2 : (image.bits == 8 ? 1 : 0);
    if (image.width <= 0 || image.height <= 0 || channels < 1 || channels > 4 || bytesPerChannel == 0)
        return false;

    const size_t pixelCount = static_cast<size_t>(image.width) * static_cast<size_t>(image.height);
    if (image.image.size() < pixelCount * channels * bytesPerChannel)
        return false;

    if (channels == 4 && bytesPerChannel == 1)
    {
        out.assign(image.image.begin(), image.image.begin() + pixelCount * 4);
        return true;
    }

    auto channel = [&](size_t pixel, int c) -> uint8_t {
        const size_t index = pixel * channels + c;
        if (bytesPerChannel == 1)
            return image.image[index];

        uint16_t value;
        std::memcpy(&value, &image.image[index * 2], sizeof(value));
        return static_cast<uint8_t>(value >> 8);
    };

    out.resize(pixelCount * 4);
    for (size_t p = 0; p < pixelCount; ++p)
    {
        uint8_t* dst = &out[p * 4];
        if (channels >= 3)
        {
            dst[0] = channel(p, 0);
            dst[1] = channel(p, 1);
            dst[2] = channel(p, 2);
            dst[3] = channels == 4 ? channel(p, 3) : 255;
        }
        else
        {
            // Grayscale, optionally with alpha.
            dst[0] = dst[1] = dst[2] = channel(p, 0);
            dst[3] = channels == 2 ? channel(p, 1) : 255;
        }
    }
    return true;
}

#pragma endregion

// One GPU mesh from all primitives of a glTF mesh. Each primitive becomes a submesh whose
// materialIndex is the primitive's index, i.e. its slot in MeshComponent::materials.
std::shared_ptr<IMesh> LoadEntireMesh(const tinygltf::Model& model, const tinygltf::Mesh& rawMesh)
{
    std::vector<GLTF::Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Submesh> submeshes;

    for (size_t primIndex = 0; primIndex < rawMesh.primitives.size(); ++primIndex)
    {
        std::vector<GLTF::Vertex> localVerts;
        std::vector<uint32_t>     localIdx;
        PrimitiveTopology topology = PrimitiveTopology::Triangles;
        LoadVertices(model, rawMesh.primitives[primIndex], localVerts, localIdx, topology);
        if(localVerts.empty() || localIdx.empty()) continue;

        const uint32_t baseVertex = (uint32_t)vertices.size();
        const uint32_t startIndex = (uint32_t)indices.size();
        vertices.insert(vertices.end(), localVerts.begin(), localVerts.end());

        indices.reserve(indices.size() + localIdx.size());
        for (uint32_t i : localIdx) indices.push_back(i + baseVertex);

        Submesh sm{};
        sm.startIndex    = startIndex;
        sm.indexCount    = (uint32_t)localIdx.size();
        sm.materialIndex = (uint32_t)primIndex;
        sm.topology      = topology;
        submeshes.push_back(sm);
    }

    if (vertices.empty() || indices.empty())
        return nullptr;

    std::shared_ptr<IMesh> mesh;
    if (vertices.size() <= 65536)
    {
        // Every index fits in 16 bits: half the index memory and bandwidth.
        std::vector<uint16_t> shortIndices(indices.begin(), indices.end());
        mesh = std::make_shared<Mesh<GLTF::Vertex, uint16_t>>(std::move(vertices), std::move(shortIndices));
    }
    else
    {
        mesh = std::make_shared<Mesh<GLTF::Vertex, uint32_t>>(std::move(vertices), std::move(indices));
    }
    mesh->submeshes = std::move(submeshes);
    mesh->BuildMesh();
    // The GPU has its own copy now; only the counts and bounds are kept on the CPU.
    mesh->Clear();
    return mesh;
}

struct LoadedMesh
{
    AssetHandle<IMesh> mesh;
    std::vector<AssetHandle<MaterialAsset>> materials; // one per primitive
};

std::string NodeName(const tinygltf::Model& model, int nodeIndex)
{
    const std::string& name = model.nodes[nodeIndex].name;
    return name.empty() ? "Node " + std::to_string(nodeIndex) : name;
}

void BuildEntityFromNode(Prefab& p, Entity e, const tinygltf::Model& model, int nodeIndex, const std::vector<LoadedMesh>& meshes,
                         const std::vector<AssetHandle<Skin>>& skins)
{
    const tinygltf::Node& n = model.nodes[nodeIndex];
    glm::vec3 translation(0.0f);
    glm::quat   rotation(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale(1.0f);

    if (n.matrix.size() == 16) {
        // Nodes may give a full matrix instead of TRS; split it so the editor can edit it.
        const glm::mat4 m = glm::mat4(glm::make_mat4(n.matrix.data()));
        translation = glm::vec3(m[3]);
        scale = glm::vec3(glm::length(glm::vec3(m[0])), glm::length(glm::vec3(m[1])), glm::length(glm::vec3(m[2])));
        // A mirroring matrix has no rotation: keep the reflection as a negative scale.
        if (glm::determinant(glm::mat3(m)) < 0.0f)
            scale.x = -scale.x;

        glm::mat3 rotationMatrix(1.0f);
        for (int axis = 0; axis < 3; ++axis)
            if (scale[axis] > 0.000001f) rotationMatrix[axis] = glm::vec3(m[axis]) / scale[axis];
        rotation = glm::normalize(glm::quat_cast(rotationMatrix));
    }

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
    e.Add<AnimationTargetEntity>({ (uint32_t)nodeIndex });

    if (n.mesh >= 0 && n.mesh < (int)meshes.size() && meshes[n.mesh].mesh.IsValid())
        e.Add<MeshComponent>(MeshComponent{ meshes[n.mesh].mesh, meshes[n.mesh].materials }).AddTag<ShadowCasterTag>();

    if (n.skin >= 0 && n.skin < (int)skins.size() && skins[n.skin].IsValid())
        e.Add<SkinComponent>(SkinComponent(skins[n.skin]));

    for (int childIndex : n.children) {
        if (childIndex < 0 || childIndex >= (int)model.nodes.size()) continue;
        Entity child = p.Instantiate(NodeName(model, childIndex).c_str());
        child.SetParent(e);
        BuildEntityFromNode(p, child, model, childIndex, meshes, skins);
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

std::shared_ptr<Prefab> GLTF::GLTFLoader::LoadGLTF(AssetManager2& assets, const std::filesystem::path& file, Shader2& shader)
{
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    const std::string filename = file.string();
    std::string extension = file.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    const bool ret = extension == ".gltf"
        ? loader.LoadASCIIFromFile(&model, &err, &warn, filename)
        : loader.LoadBinaryFromFile(&model, &err, &warn, filename);

    if (!warn.empty()) {
      Logger::Warning(warn);
    }

    if (!err.empty()) {
      Logger::Error(err);
      return nullptr;
    }

    if (!ret) {
      Logger::Error("Failed to parse glTF: " + filename);
      return nullptr;
    }

    if (model.scenes.empty()) {
      Logger::Error("glTF has no scenes: " + filename);
      return nullptr;
    }

    const int sceneIndex = (model.defaultScene >= 0 && model.defaultScene < (int)model.scenes.size()) ? model.defaultScene : 0;
    const tinygltf::Scene& gltfScene = model.scenes[sceneIndex];
    if (gltfScene.nodes.empty()) {
      Logger::Error("glTF scene has no nodes: " + filename);
      return nullptr;
    }

    const std::string fileKey = AssetManager2::MakeKey(file);
    const std::string fileName = file.stem().string();
    auto subKey = [&](const char* kind, size_t index) {
        return fileKey + "#" + kind + "/" + std::to_string(index);
    };
    auto subName = [&](const std::string& name, const char* kind, size_t index) {
        return fileName + "/" + (name.empty() ? std::string(kind) + "_" + std::to_string(index) : name);
    };

    AssetHandle<Texture> whiteSrgb = BuiltinAssets::WhiteTexture(assets, true);
    AssetHandle<Texture> whiteLinear = BuiltinAssets::WhiteTexture(assets, false);
    AssetHandle<Texture> flatNormal = BuiltinAssets::FlatNormalTexture(assets);

    // Textures are made on demand per (image, color space): base color and emissive hold
    // sRGB-encoded color, the other maps hold linear data.
    auto loadTexture = [&](int textureIndex, bool srgb, const AssetHandle<Texture>& fallback) -> AssetHandle<Texture> {
        if (textureIndex < 0 || textureIndex >= (int)model.textures.size())
            return fallback;

        const int imageIndex = model.textures[textureIndex].source;
        if (imageIndex < 0 || imageIndex >= (int)model.images.size())
            return fallback;

        const std::string key = subKey("image", imageIndex) + (srgb ? "/srgb" : "/linear");
        if (AssetHandle<Texture> existing = assets.Find<Texture>(key); existing.IsValid())
            return existing;

        const tinygltf::Image& image = model.images[imageIndex];
        std::vector<uint8_t> rgba;
        if (!ToRGBA8(image, rgba)) {
            Logger::Warning("Unsupported image format for image " + std::to_string(imageIndex) + " in " + filename);
            return fallback;
        }

        // Mipmapped: minified textures sample from small levels instead of thrashing the cache.
        auto texture = std::make_shared<Texture>();
        texture->CreateTexture(image.width, image.height, srgb ? TextureFormat::RGBA8UnormSrgb : TextureFormat::RGBA8Unorm,
                               false, false, false, Texture::MipLevelCount(image.width, image.height));
        texture->UploadTextureWithMips(rgba.data(), rgba.size(), image.width, image.height);
        return assets.RegisterAsset<Texture>(texture, AssetType::Texture, subName(image.name, "image", imageIndex), key);
    };

    // A texture's glTF sampler; repeat and trilinear when it has none (or leaves filters open).
    auto samplerFor = [&](int textureIndex) {
        SamplerSettings settings;
        if (textureIndex < 0 || textureIndex >= (int)model.textures.size())
            return settings;
        const int samplerIndex = model.textures[textureIndex].sampler;
        if (samplerIndex < 0 || samplerIndex >= (int)model.samplers.size())
            return settings;

        const tinygltf::Sampler& sampler = model.samplers[samplerIndex];
        auto address = [](int wrap) {
            return wrap == TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE ? SamplerAddress::ClampToEdge
                 : wrap == TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT ? SamplerAddress::MirrorRepeat : SamplerAddress::Repeat;
        };
        settings.addressU = address(sampler.wrapS);
        settings.addressV = address(sampler.wrapT);
        if (sampler.magFilter == TINYGLTF_TEXTURE_FILTER_NEAREST)
            settings.magFilter = SamplerFilter::Nearest;
        switch (sampler.minFilter)
        {
            case TINYGLTF_TEXTURE_FILTER_NEAREST:                settings.minFilter = SamplerFilter::Nearest; settings.mipmaps = false; break;
            case TINYGLTF_TEXTURE_FILTER_LINEAR:                 settings.mipmaps = false; break;
            case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST: settings.minFilter = SamplerFilter::Nearest; settings.mipmapFilter = SamplerFilter::Nearest; break;
            case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:  settings.mipmapFilter = SamplerFilter::Nearest; break;
            case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:  settings.minFilter = SamplerFilter::Nearest; break;
            default: break;
        }
        return settings;
    };

    std::vector<AssetHandle<MaterialAsset>> materials(model.materials.size());
    for (size_t i = 0; i < model.materials.size(); ++i)
    {
        const std::string key = subKey("material", i);
        if (AssetHandle<MaterialAsset> existing = assets.Find<MaterialAsset>(key); existing.IsValid()) {
            materials[i] = existing;
            continue;
        }

        const tinygltf::Material& mat = model.materials[i];
        const tinygltf::PbrMetallicRoughness& pbr = mat.pbrMetallicRoughness;

        AssetHandle<Texture> albedo = loadTexture(pbr.baseColorTexture.index, true, whiteSrgb);
        AssetHandle<Texture> normal = loadTexture(mat.normalTexture.index, false, flatNormal);
        AssetHandle<Texture> metallicRoughness = loadTexture(pbr.metallicRoughnessTexture.index, false, whiteLinear);
        AssetHandle<Texture> occlusion = loadTexture(mat.occlusionTexture.index, false, whiteLinear);
        AssetHandle<Texture> emissive = loadTexture(mat.emissiveTexture.index, true, whiteSrgb);

        auto material = std::make_shared<MaterialAsset>();
        GLTFMaterialProperties& props = material->properties;
        if (pbr.baseColorFactor.size() == 4) {
            props.baseColor = glm::vec4(pbr.baseColorFactor[0], pbr.baseColorFactor[1],
                                        pbr.baseColorFactor[2], pbr.baseColorFactor[3]);
        }
        props.metallicFactor = (float)pbr.metallicFactor;
        props.roughnessFactor = (float)pbr.roughnessFactor;
        props.normalMapStrength = (float)mat.normalTexture.scale;
        props.occlusionStrength = (float)mat.occlusionTexture.strength;
        if (mat.emissiveFactor.size() == 3) {
            props.emissiveFactor = glm::vec3(mat.emissiveFactor[0], mat.emissiveFactor[1], mat.emissiveFactor[2]);
        }
        // KHR_materials_emissive_strength scales emission past 1 (HDR), which is what makes it bloom.
        auto emissiveStrength = mat.extensions.find("KHR_materials_emissive_strength");
        if (emissiveStrength != mat.extensions.end() && emissiveStrength->second.Has("emissiveStrength")) {
            props.emissiveFactor *= (float)emissiveStrength->second.Get("emissiveStrength").GetNumberAsDouble();
        }
        // Opaque materials ignore alpha. Blending isn't supported, so BLEND is treated like MASK.
        props.alphaCutoff = mat.alphaMode == "OPAQUE" ? 0.0f : (float)mat.alphaCutoff;
        material->doubleSided = mat.doubleSided;

        // Which UV set each texture reads; only TEXCOORD_0 and TEXCOORD_1 are loaded.
        const int texCoords[5] = { pbr.baseColorTexture.texCoord, mat.normalTexture.texCoord, pbr.metallicRoughnessTexture.texCoord,
                                   mat.occlusionTexture.texCoord, mat.emissiveTexture.texCoord };
        for (uint32_t t = 0; t < 5; ++t) {
            if (texCoords[t] == 1)
                props.uvSets |= 1u << t;
            else if (texCoords[t] > 1)
                Logger::Warning("Material " + std::to_string(i) + " in " + filename + " uses TEXCOORD_" + std::to_string(texCoords[t]) + "; only 0 and 1 are supported");
        }

        material->material.InitFromShader(shader)
              .SetTexture("albedoMap", *albedo.Get())
              .SetTexture("normalMap", *normal.Get())
              .SetTexture("occlusionMap", *occlusion.Get())
              .SetTexture("metallicRoughnessMap", *metallicRoughness.Get())
              .SetTexture("emissiveMap", *emissive.Get())
              .SetSampler("albedoSampler", samplerFor(pbr.baseColorTexture.index))
              .SetSampler("normalSampler", samplerFor(mat.normalTexture.index))
              .SetSampler("metallicRoughnessSampler", samplerFor(pbr.metallicRoughnessTexture.index))
              .SetSampler("occlusionSampler", samplerFor(mat.occlusionTexture.index))
              .SetSampler("emissiveSampler", samplerFor(mat.emissiveTexture.index))
              .Build();
        material->textures = { albedo, normal, metallicRoughness, occlusion, emissive };

        materials[i] = assets.RegisterAsset<MaterialAsset>(material, AssetType::Material, subName(mat.name, "material", i), key);
    }

    AssetHandle<MaterialAsset> defaultMaterial = BuiltinAssets::DefaultMaterial(assets, shader);

    std::vector<LoadedMesh> meshes(model.meshes.size());
    for (size_t i = 0; i < model.meshes.size(); ++i)
    {
        const tinygltf::Mesh& rawMesh = model.meshes[i];
        for (const tinygltf::Primitive& prim : rawMesh.primitives) {
            const bool hasMaterial = prim.material >= 0 && prim.material < (int)materials.size();
            meshes[i].materials.push_back(hasMaterial ? materials[prim.material] : defaultMaterial);
        }

        const std::string key = subKey("mesh", i);
        if (AssetHandle<IMesh> existing = assets.Find<IMesh>(key); existing.IsValid()) {
            meshes[i].mesh = existing;
            continue;
        }

        if (std::shared_ptr<IMesh> mesh = LoadEntireMesh(model, rawMesh))
            meshes[i].mesh = assets.RegisterAsset<IMesh>(mesh, AssetType::Mesh, subName(rawMesh.name, "mesh", i), key);
    }

    std::vector<AssetHandle<Skin>> skins(model.skins.size());
    for (size_t i = 0; i < model.skins.size(); ++i)
    {
        const std::string key = subKey("skin", i);
        if (AssetHandle<Skin> existing = assets.Find<Skin>(key); existing.IsValid()) {
            skins[i] = existing;
            continue;
        }

        const tinygltf::Skin& rawSkin = model.skins[i];
        auto skin = std::make_shared<Skin>();
        skin->jointNodes.assign(rawSkin.joints.begin(), rawSkin.joints.end());
        skin->inverseBind.assign(rawSkin.joints.size(), glm::mat4(1.0f));

        if (rawSkin.inverseBindMatrices >= 0 && rawSkin.inverseBindMatrices < (int)model.accessors.size())
        {
            const std::vector<glm::mat4> matrices = ReadAccessor<glm::mat4>(model, model.accessors[rawSkin.inverseBindMatrices]);
            std::copy_n(matrices.begin(), std::min(matrices.size(), skin->inverseBind.size()), skin->inverseBind.begin());
        }

        skins[i] = assets.RegisterAsset<Skin>(skin, AssetType::Skin, subName(rawSkin.name, "skin", i), key);
    }

    std::vector<AssetHandle<Animation>> clips;
    for (size_t i = 0; i < model.animations.size(); ++i)
    {
        const std::string key = subKey("animation", i);
        AssetHandle<Animation> clip = assets.Find<Animation>(key);
        if (!clip.IsValid()) {
            try {
                auto animation = std::make_shared<Animation>(LoadAnimation(model, model.animations[i]));
                clip = assets.RegisterAsset<Animation>(animation, AssetType::Animation, subName(animation->GetName(), "animation", i), key);
            } catch (const std::exception& e) {
                Logger::Warning("Skipping animation " + std::to_string(i) + " in " + filename + ": " + e.what());
                continue;
            }
        }
        clips.push_back(clip);
    }

    // The prefab root is an extra entity above the glTF scene's nodes, so an instance can be
    // placed freely while every glTF node (including animated roots) keeps its own transform.
    auto prefab = std::make_shared<Prefab>(fileName.c_str());
    Entity root = prefab->Root();

    if (!clips.empty()) {
        AnimationPlayer player;
        player.clips = std::move(clips);
        player.currentClip = 0;
        root.Add<AnimationPlayer>(player);
    }

    for (int nodeIndex : gltfScene.nodes)
    {
        if (nodeIndex < 0 || nodeIndex >= (int)model.nodes.size()) continue;
        Entity node = prefab->Instantiate(NodeName(model, nodeIndex).c_str());
        node.SetParent(root);
        BuildEntityFromNode(*prefab, node, model, nodeIndex, meshes, skins);
    }

    return prefab;
}

void GLTF::GLTFLoader::RegisterLoader(AssetManager2& assets, Shader2& shader)
{
    assets.RegisterLoader<Prefab>(AssetType::Prefab, { ".gltf", ".glb" },
        [&shader](AssetManager2& manager, const AssetMetadata& meta) {
            return LoadGLTF(manager, meta.path, shader);
        });
}
