#include "GLTFLoader.hpp"
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../External/tiny_gltf.h"

#include "../Logging.hpp"
#include "../Components/Relationship.hpp"
#include "../Components/Transform.hpp"
#include "../Components/Mesh.hpp"
#include "../Components/Material.hpp"
#include "../Components/RendererComponent.hpp"
#include "../GlobalVaribles.hpp"

#include <vector>

template <typename T>
std::vector<T> ReadAccessorData(const tinygltf::Model& model, const tinygltf::Accessor& accessor) {
    // Get the buffer view and buffer for the accessor.
    const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
    const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

    // Compute pointer to the start of the data.
    const unsigned char* bufferStart = buffer.data.data() + bufferView.byteOffset;
    const unsigned char* accessorStart = bufferStart + accessor.byteOffset;

    // Determine stride. If not provided, assume tightly packed data.
    size_t stride = accessor.ByteStride(bufferView);
    if (stride == 0) {
        stride = sizeof(T);
    }

    std::vector<T> data;
    data.reserve(accessor.count);

    // Loop over each element, copying the value of type T.
    for (size_t i = 0; i < accessor.count; ++i) {
        // Calculate the address of the current element.
        const T* elementPtr = reinterpret_cast<const T*>(accessorStart + i * stride);
        data.push_back(*elementPtr);
    }
    return data;
}


std::vector<Mesh> GenerateMeshes(tinygltf::Model& model, Shader& shader, int materialOffset)
{
  std::vector<Mesh> res;
  size_t count = model.meshes.size();

  for(size_t i = 0; i < count; i++)
  {
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> normals;
    std::vector<uint16_t> indices;
    std::vector<Submesh> subs;
    for (const auto& primitive : model.meshes[i].primitives) {
      Submesh subMesh;
      subMesh.materialIndex = primitive.material == -1 ? -1 : primitive.material + materialOffset;
      subMesh.startIndex = indices.size();
      subMesh.startVertex = vertices.size();

      if (primitive.attributes.find("POSITION") != primitive.attributes.end()) {
        int posAccessorIndex = primitive.attributes.at("POSITION");
        const tinygltf::Accessor& posAccessor = model.accessors[posAccessorIndex];
        std::vector<glm::vec3> newPositions = ReadAccessorData<glm::vec3>(model, posAccessor);
        vertices.insert(vertices.end(), newPositions.begin(), newPositions.end());
      }

      if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
        int normalAccessorIndex = primitive.attributes.at("NORMAL");
        const tinygltf::Accessor& normalAccessor = model.accessors[normalAccessorIndex];
        std::vector<glm::vec3> newNormals = ReadAccessorData<glm::vec3>(model, normalAccessor);
        normals.insert(normals.end(), newNormals.begin(), newNormals.end());
      }
      if (primitive.indices > -1) {
        const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
        if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
            std::vector<uint16_t> newIndices = ReadAccessorData<uint16_t>(model, indexAccessor);
            indices.insert(indices.end(), newIndices.begin(), newIndices.end());
        }
        else {
            Logging::PrintError("Failed to read indices");
        }
      }
      subMesh.indexxCount = indices.size() - subMesh.startIndex;
      subMesh.vertexCount = vertices.size() - subMesh.startVertex;
      subs.push_back(subMesh);
    }
    assert(vertices.size() == normals.size());
    Mesh mesh;
    mesh.submeshes = subs;
    mesh.indexCount = indices.size();
    mesh.shaderIndex = 0;
    std::vector<VertexAttribute> attributes = 
    {
        {"POSITION", vertices.data(), vertices.size() },
        {"NORMAL", normals.data(), normals.size() }
    };
    mesh.vertexBuffer = shader.CreateVertexBuffer(attributes);

    wgpu::BufferDescriptor bufferDesc;
    bufferDesc.mappedAtCreation = false;
    bufferDesc.size = indices.size() * sizeof(uint16_t);
    bufferDesc.size = (bufferDesc.size + 3) & ~3;
    bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Index;

    mesh.indexBuffer = device.CreateBuffer(&bufferDesc);
    device.GetQueue().WriteBuffer(mesh.indexBuffer, 0, indices.data(), bufferDesc.size);
    res.push_back(mesh);
  }

  return res;
}

std::vector<MaterialComponent> LoadMaterials(tinygltf::Model& model, int offset)
{
  std::vector<MaterialComponent> res;
  size_t materialCount = model.materials.size();

  for(int i = 0; i < materialCount; i++)
  {
    MaterialComponent mat;
    mat.uniform.baseColorFactor = {(float)model.materials[i].pbrMetallicRoughness.baseColorFactor[0],
                                  (float)model.materials[i].pbrMetallicRoughness.baseColorFactor[1],
                                  (float)model.materials[i].pbrMetallicRoughness.baseColorFactor[2],
                                  (float)model.materials[i].pbrMetallicRoughness.baseColorFactor[3]};
    mat.uniform.metallicFactor = (float)model.materials[i].pbrMetallicRoughness.metallicFactor;
    mat.uniform.roughnessFactor = (float)model.materials[i].pbrMetallicRoughness.roughnessFactor;
    mat.uniform.emissiveFactor = {(float)model.materials[i].emissiveFactor[0],
                                  (float)model.materials[i].emissiveFactor[1],
                                  (float)model.materials[i].emissiveFactor[2]};
    mat.dataIndex = i + offset;
    res.push_back(mat);
  }

  return res;
}

void LoadGLTFObject(std::string name, Scene& scene, RenderSystem& rendersystem)
{
    std::string fullpath = std::string(RESOURCE_DIR) + "/" + name;
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;
    bool ret;
    
    ret = loader.LoadBinaryFromFile(&model, &err, &warn, fullpath);

    if (!warn.empty()) {
      printf("Warn: %s\n", warn.c_str());
    }

    if (!err.empty()) {
      printf("Err: %s\n", err.c_str());
    }

    if (!ret) {
      printf("Failed to parse glTF\n");
      return;
    }
    auto materials = LoadMaterials(model, rendersystem.GetMaterialCount());
    
    auto meshes = GenerateMeshes(model, rendersystem.GetShaders()[0], rendersystem.GetMaterialCount());
    rendersystem.AddMaterials(materials);
    std::vector<entt::entity> entities;
    entities.reserve(model.nodes.size());
    for(auto& node : model.nodes)
    {
        entities.push_back(scene.CreateGameobject(node.name));
    }

    for(int i = 0; i < entities.size(); i++)
    {
        auto& currentNode = scene.GetEntities().get<Relationship>(entities[i]);
        auto& transform = scene.GetEntities().get<Transform>(entities[i]);
        transform.position = model.nodes[i].translation.size() == 0 ? glm::vec3(0.0f) : glm::vec3((float)model.nodes[i].translation[0], (float)model.nodes[i].translation[1], (float)model.nodes[i].translation[2]);
        transform.scale = model.nodes[i].scale.size() == 0 ? glm::vec3(1.0f) : glm::vec3((float)model.nodes[i].scale[0], (float)model.nodes[i].scale[1], (float)model.nodes[i].scale[2]);
        
        transform.rotation = model.nodes[i].rotation.size() == 0 ? glm::quat(1.0f, 0.0f, 0.0f, 0.0f) : glm::quat(static_cast<float>(model.nodes[i].rotation[3]), 
        static_cast<float>(model.nodes[i].rotation[0]), 
        static_cast<float>(model.nodes[i].rotation[1]),
        static_cast<float>(model.nodes[i].rotation[2]));  

        transform.data.modelMatrix = glm::translate(glm::mat4(1.0f), transform.position) *
                                     glm::mat4_cast(transform.rotation) *
                                     glm::scale(glm::mat4(1.0f), transform.scale);

        glm::mat3 normalMat3 = glm::transpose(glm::inverse(glm::mat3(transform.data.modelMatrix)));
        glm::mat4 normalMatrix = glm::mat4(1.0f); 
        normalMatrix[0] = glm::vec4(normalMat3[0], 0.0f); 
        normalMatrix[1] = glm::vec4(normalMat3[1], 0.0f); 
        normalMatrix[2] = glm::vec4(normalMat3[2], 0.0f); 
        transform.data.normalMatrix = normalMatrix;

        if(model.nodes[i].mesh != -1) 
        {
          auto& r = scene.GetEntities().emplace<Mesh>(entities[i]);
          r = meshes[model.nodes[i].mesh];
          
        }
        rendersystem.RegisterComponent(scene.GetEntities(), entities[i]);
        if(!scene.GetEntities().all_of<Dirty>(entities[i])) {
          scene.GetEntities().emplace<Dirty>(entities[i]);
      }
        for (int childIndex : model.nodes[i].children)
            currentNode.AddChild(entities[i], entities[childIndex], scene.GetEntities());
    }
    
    Logging::PrintSuccess("Succesfully loaded a gltf model");
}