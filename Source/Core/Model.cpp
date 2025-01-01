#include "Model.hpp"
#include <iostream>

#include "Mesh.hpp"

NodesMesh LoadGLTFPrimitives(tinygltf::Model& model, int index)
{
    std::vector<Vertex> vertexData;
    std::vector<uint16_t> indices;
    int matIndex = -1;

    for (const auto& primitive : model.meshes[index].primitives) {
        matIndex = primitive.material;
        // Extract position data
        std::vector<glm::vec3> positions;
        if (primitive.attributes.find("POSITION") != primitive.attributes.end()) {
            int posAccessorIndex = primitive.attributes.at("POSITION");
            const tinygltf::Accessor& posAccessor = model.accessors[posAccessorIndex];
            const tinygltf::BufferView& posBufferView = model.bufferViews[posAccessor.bufferView];
            const tinygltf::Buffer& posBuffer = model.buffers[posBufferView.buffer];

            const float* posData = reinterpret_cast<const float*>(&posBuffer.data[posBufferView.byteOffset]);
            size_t numVertices = posAccessor.count;
            vertexData.reserve(numVertices);
            for (size_t i = 0; i < numVertices; ++i) {
                positions.push_back(glm::vec3(posData[i * 3 + 0], posData[i * 3 + 1], posData[i * 3 + 2]));
            }
        }

        std::cout << "Loading positions" << std::endl;

        std::vector<glm::vec3> normals;
        if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
            int normalAccessorIndex = primitive.attributes.at("NORMAL");
            const tinygltf::Accessor& normalAccessor = model.accessors[normalAccessorIndex];
            const tinygltf::BufferView& normalBufferView = model.bufferViews[normalAccessor.bufferView];
            const tinygltf::Buffer& normalBuffer = model.buffers[normalBufferView.buffer];

            const float* normalData = reinterpret_cast<const float*>(&normalBuffer.data[normalBufferView.byteOffset]);
            size_t numNormals = normalAccessor.count;

            for (size_t i = 0; i < numNormals; ++i) {
                normals.push_back(glm::vec3(normalData[i * 3 + 0], normalData[i * 3 + 1], normalData[i * 3 + 2]));
            }
        }

        std::cout << "Loading normals" << std::endl;

        std::vector<glm::vec2> uvs;
        if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
            int uvAccessorIndex = primitive.attributes.at("TEXCOORD_0");
            const tinygltf::Accessor& uvAccessor = model.accessors[uvAccessorIndex];
            const tinygltf::BufferView& uvBufferView = model.bufferViews[uvAccessor.bufferView];
            const tinygltf::Buffer& uvBuffer = model.buffers[uvBufferView.buffer];

            const float* uvData = reinterpret_cast<const float*>(&uvBuffer.data[uvBufferView.byteOffset]);
            size_t numUVs = uvAccessor.count;

            for (size_t i = 0; i < numUVs; ++i) {
                uvs.push_back(glm::vec2(uvData[i * 2 + 0], uvData[i * 2 + 1] - 1));
            }
        }
        std::cout << "Loading uvs" << std::endl;

        std::vector<glm::vec4> colors;
        if (primitive.attributes.find("COLOR_0") != primitive.attributes.end()) {
            int colorAccessorIndex = primitive.attributes.at("COLOR_0");
            const tinygltf::Accessor& colorAccessor = model.accessors[colorAccessorIndex];
            const tinygltf::BufferView& colorBufferView = model.bufferViews[colorAccessor.bufferView];
            const tinygltf::Buffer& colorBuffer = model.buffers[colorBufferView.buffer];

            const float* colorData = reinterpret_cast<const float*>(&colorBuffer.data[colorBufferView.byteOffset]);
            size_t numColors = colorAccessor.count;

            for (size_t i = 0; i < numColors; ++i) {
                colors.push_back(glm::vec4(colorData[i * 4 + 0], colorData[i * 4 + 1], colorData[i * 4 + 2], colorData[i * 4 + 3]));
            }
        }

        std::cout << "Loading colors" << std::endl;

        size_t numVertices = positions.size();
        if (normals.size() != numVertices/* || uvs.size() != numVertices*/) {
            std::cerr << "Error: Mismatch in number of positions, normals, or UVs\n";
            return {};
        }
        else
            std::cout << "Mesh created succesfully" << std::endl;

        for (size_t i = 0; i < numVertices; ++i) {
            Vertex v = {};
            v.position = positions[i];
            v.normal = normals[i];
            if(colors.size() > 0)
                v.color = colors[i];
            if(colors.size() > 0)
                v.uv = uvs[i];
            vertexData.push_back(v);
        }
        
        if (primitive.indices > -1) {
            int indicesAccessorIndex = primitive.indices;
            const tinygltf::Accessor& indicesAccessor = model.accessors[indicesAccessorIndex];
            const tinygltf::BufferView& indicesBufferView = model.bufferViews[indicesAccessor.bufferView];
            const tinygltf::Buffer& indicesBuffer = model.buffers[indicesBufferView.buffer];

            if (indicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                const uint16_t* indicesData = reinterpret_cast<const uint16_t*>(&indicesBuffer.data[indicesBufferView.byteOffset]);
                size_t numIndices = indicesAccessor.count;

                for (size_t i = 0; i < numIndices; ++i) {
                    indices.push_back(indicesData[i]);
                }
            } else {
                std::cerr << "Unsupported index component type: " << indicesAccessor.componentType << std::endl;
            }
        }   
    }
    NodesMesh newMesh;
    wgpu::BufferDescriptor bufferDesc;
    bufferDesc.size = vertexData.size() * sizeof(Vertex);
    bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex;
    bufferDesc.mappedAtCreation = false;
    newMesh.vertexBuffer = device.CreateBuffer(&bufferDesc);
    device.GetQueue().WriteBuffer(newMesh.vertexBuffer, 0, vertexData.data(), bufferDesc.size);

    bufferDesc.size = indices.size() * sizeof(uint16_t);
    bufferDesc.size = (bufferDesc.size + 3) & ~3;
    bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Index;
    
    newMesh.indexBuffer = device.CreateBuffer(&bufferDesc);
    device.GetQueue().WriteBuffer(newMesh.indexBuffer, 0, indices.data(), bufferDesc.size); 
    newMesh.indexCount = indices.size();
    newMesh.matIndex = matIndex;
    return newMesh;
}


Model::Model(std::string name)
{
    
    std::string fullpath = std::string(RESOURCE_DIR) + "/" + name;

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, fullpath);

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
    
    for (int i = 0; i < model.meshes.size(); i++)
    {
        meshes.push_back(LoadGLTFPrimitives(model, i));
    }
    
    //gameObject = GameObject(model.meshes[0].name, mesh, bindGroup);
    rootNode = LoadNodes(model);
    textures.resize(model.images.size());
    InitUniforms(model);
    return;

    
    // for (tinygltf::Material& m : model.materials)
    //     LoadMaterial(m);

    int i = 0;
    for (tinygltf::Image& m : model.images)
    {
        LoadTexture(m, i);
        i++;
    }

    std::cout << "Succesfully loaded GLTF: " << model.meshes[0].name << std::endl;
}

Node* Model::LoadNodes(tinygltf::Model& m)
{
    
    int i = 0;
    for (tinygltf::Node& node : m.nodes)
    {
        Node* newNode = new Node();
        newNode->name = node.name;
        if (node.translation.size() == 3)
        {
            newNode->localPosition = glm::vec3(static_cast<float>(node.translation[0]), 
                     static_cast<float>(node.translation[1]), 
                     static_cast<float>(node.translation[2]));
        }
        else
        {
            newNode->localPosition = {0,0,0};
        }

        if (node.scale.size() == 3)
        {
        newNode->localScale = glm::vec3(static_cast<float>(node.scale[0]), 
                     static_cast<float>(node.scale[1]), 
                     static_cast<float>(node.scale[2]));  
        }
        else
        {
            newNode->localScale = {1,1,1};
        }

        if (node.rotation.size() == 4)
        {
            newNode->localRotation = glm::quat(static_cast<float>(node.rotation[0]), 
                     static_cast<float>(node.rotation[1]), 
                     static_cast<float>(node.rotation[2]),
                     static_cast<float>(node.rotation[3]));  
        }
        else
        {
            newNode->localRotation = {1,0,0,0};
        }

        glm::mat4x4 modelMatrix = glm::translate(glm::mat4(1.0f), newNode->localPosition);/*
                          glm::mat4_cast(newNode->localRotation) *
                          glm::scale(glm::mat4(1.0f), newNode->localScale);  */
        //glm::mat4x4 modelMatrix = glm::translate(glm::mat4(1.0f), newNode->localPosition);
        
        // glm::mat4x4 modelMatrix = glm::mat4(1.0f); // Start with an identity matrix
        // modelMatrix = glm::rotate(modelMatrix, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)); // Rotate 90 degrees on X-axis
        // modelMatrix = glm::rotate(modelMatrix, glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        // modelMatrix = glm::scale(modelMatrix, glm::vec3(1.0f, 1.0f, 1.0f));
        // modelMatrix = glm::translate(modelMatrix, glm::vec3(0.0f, -1.0f, 0.0f));
        newNode->modelMatrix = modelMatrix;
        if (node.mesh != -1)
            meshes[node.mesh].nodeIndex = i;
        //newNode->meshIndex = node.mesh;
        nodes.push_back(newNode);
        i++;
    }
    for (int i = 0; i < m.nodes.size(); i++)
    {
        for (int index : m.nodes[i].children)
        {
            nodes[i]->children.push_back(nodes[index]);
            nodes[index]->parent = nodes[i];
        }
    }

    Node* rootNode = nodes[0];
    while (rootNode->parent != nullptr)
    {
        rootNode = rootNode->parent;
    }
    return rootNode;
}

void Model::LoadTexture(tinygltf::Image& img, int i)
{
    using namespace wgpu;
    if (textures[i].bindingIndex == -1)
        return;
    TextureFormat textureFormat = TextureFormat::RGBA8UnormSrgb;
    if(textures[i].bindingIndex != 2) textureFormat = TextureFormat::RGBA8Unorm;
    
    int width = img.width;
    int height = img.height;
    int channels = img.component;
    unsigned char* imageData = img.image.data();
    if (imageData == nullptr) {
        // Handle error if image loading failed
        std::cerr << "Failed to load texture from gltf model!" << std::endl;
        return;
    }
    std::vector<uint8_t> pixels(channels * width * height);
    std::memcpy(pixels.data(), imageData, pixels.size());
    TextureDescriptor textureDesc;
    textureDesc.dimension = TextureDimension::e2D;
    textureDesc.format = textureFormat;
    textureDesc.mipLevelCount = 1;
    textureDesc.sampleCount = 1;
    textureDesc.size = {static_cast<unsigned int>(width), static_cast<unsigned int>(height), 1};
    textureDesc.usage = TextureUsage::TextureBinding | TextureUsage::CopyDst;
    textureDesc.viewFormatCount = 1;
    textureDesc.viewFormats = &textureFormat;
    Texture texture = device.CreateTexture(&textureDesc);
    TextureViewDescriptor textureViewDesc;
    textureViewDesc.aspect = TextureAspect::All;
    textureViewDesc.baseArrayLayer = 0;
    textureViewDesc.arrayLayerCount = 1;
    textureViewDesc.baseMipLevel = 0;
    textureViewDesc.mipLevelCount = 1;
    textureViewDesc.dimension = TextureViewDimension::e2D;
    textureViewDesc.format = textureFormat;
    TextureView textureView = texture.CreateView(&textureViewDesc);
	ImageCopyTexture destination;
	destination.texture = texture;
	destination.mipLevel = 0;
	destination.origin = { 0, 0, 0 };
	destination.aspect = TextureAspect::All;
	TextureDataLayout source;
	source.offset = 0;
	source.bytesPerRow = 4 * textureDesc.size.width;
	source.rowsPerImage = textureDesc.size.height;
    device.GetQueue().WriteTexture(&destination, pixels.data(), pixels.size(), &source, &textureDesc.size);

    bindings[i].binding = i;
    bindings[i].textureView = textureView;
}

void Model::InitUniforms(tinygltf::Model& model)
{
    for(tinygltf::Material& mat : model.materials) 
    {
        MaterialProperties newMaterial;
        newMaterial.alphaCutoff = mat.alphaCutoff;
        newMaterial.metallicFactor = mat.pbrMetallicRoughness.metallicFactor;
        newMaterial.roughnessFactor = mat.pbrMetallicRoughness.roughnessFactor;
        newMaterial.emissiveFactor = glm::vec3(
            static_cast<float>(mat.emissiveFactor[0]),
            static_cast<float>(mat.emissiveFactor[1]),
            static_cast<float>(mat.emissiveFactor[2])
        );

        newMaterial.baseColorFactor = glm::vec4(
            static_cast<float>(mat.pbrMetallicRoughness.baseColorFactor[0]),
            static_cast<float>(mat.pbrMetallicRoughness.baseColorFactor[1]),
            static_cast<float>(mat.pbrMetallicRoughness.baseColorFactor[2]),
            static_cast<float>(mat.pbrMetallicRoughness.baseColorFactor[3])
        );

        materialProps.push_back(newMaterial);
    }
    std::cout << std::endl;
    for (NodesMesh& NodeMesh : meshes)
    {
        ModelData data;
        
        data.modelMatrix = nodes[NodeMesh.nodeIndex]->modelMatrix;
        data.materialProps = materialProps[NodeMesh.matIndex];


        // if (mat.pbrMetallicRoughness.baseColorTexture.index != -1)
        //     textures[mat.pbrMetallicRoughness.baseColorTexture.index].bindingIndex = 2;
        // if (mat.pbrMetallicRoughness.metallicRoughnessTexture.index != -1)
        //     textures[mat.pbrMetallicRoughness.metallicRoughnessTexture.index].bindingIndex = 3;
        // if (mat.occlusionTexture.index != -1)
        //     textures[mat.occlusionTexture.index].bindingIndex = 4;

        LoadedModels.push_back(data);
    }

    using namespace wgpu;

    SupportedLimits supportedLimits;
    device.GetLimits(&supportedLimits);
    Limits deviceLimits = supportedLimits.limits;

    uniformStride = ceilToNextMultiple(
        (uint32_t)sizeof(ModelData),
        (uint32_t)deviceLimits.minUniformBufferOffsetAlignment
    );

    BufferDescriptor bufferDesc;
    bufferDesc.size = uniformStride * (meshes.size() - 1) + sizeof(ModelData);
    bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Uniform;
    bufferDesc.mappedAtCreation = false;
    modelsBuffer = device.CreateBuffer(&bufferDesc);

    for (int i = 0; i < meshes.size(); i++)
        device.GetQueue().WriteBuffer(modelsBuffer, 0, &LoadedModels[i], sizeof(ModelData));
    //device.GetQueue().WriteBuffer(modelsBuffer, uniformStride, &LoadedModels[1], sizeof(ModelData));

    BindGroupEntry entry = {};
    entry = {};
    entry.binding = 0;
    entry.buffer = modelsBuffer;
    entry.offset = 0;
    entry.size = sizeof(ModelData);

    std::vector<BindGroupLayoutEntry> modelBindingLayouts(1);
    modelBindingLayouts[0] = {};
    modelBindingLayouts[0].binding = 0;
    modelBindingLayouts[0].visibility = ShaderStage::Vertex | ShaderStage::Fragment;
    modelBindingLayouts[0].buffer.type = BufferBindingType::Uniform;
    modelBindingLayouts[0].buffer.hasDynamicOffset = true;
    modelBindingLayouts[0].buffer.minBindingSize = sizeof(ModelData);

    BindGroupLayoutDescriptor bindGroupLayoutDesc2{};
    bindGroupLayoutDesc2.entryCount = (uint32_t)modelBindingLayouts.size();
    bindGroupLayoutDesc2.entries = modelBindingLayouts.data();
    wgpu::BindGroupLayout bindGroupLayout = device.CreateBindGroupLayout(&bindGroupLayoutDesc2);

    BindGroupDescriptor bindGroupDesc{};
    bindGroupDesc.layout = bindGroupLayout;
    bindGroupDesc.entryCount = 1;
    bindGroupDesc.entries = &entry;
    bindGroup = device.CreateBindGroup(&bindGroupDesc);
    std::cout << "Created bindgroup inside model class with modeldata struct" << std::endl;
}

void Model::Draw(wgpu::RenderPassEncoder& renderPass)
{
    for(int i = 0; i < meshes.size(); i++)
    {
        uint32_t dynamicOffset = i * uniformStride;
        renderPass.SetVertexBuffer(0, meshes[i].vertexBuffer, 0, meshes[i].vertexBuffer.GetSize());
        renderPass.SetIndexBuffer(meshes[i].indexBuffer, wgpu::IndexFormat::Uint16, 0, meshes[i].indexBuffer.GetSize());
        renderPass.SetBindGroup(1, bindGroup, 1, &dynamicOffset);
        renderPass.DrawIndexed(meshes[i].indexCount, 1, 0, 0);
    }
    std::cout << std::endl;
}

Model::~Model()
{
    std::cout << "Unloading model with name IDK" << std::endl;
}