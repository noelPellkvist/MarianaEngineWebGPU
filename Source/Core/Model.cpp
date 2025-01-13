#include "Model.hpp"
#include "Resources.h"
#include <iostream>
#include <gtc/type_ptr.hpp>
#include <gtc/matrix_transform.hpp> 
#include <gtc/matrix_inverse.hpp> 


Model::Model(std::string name, bool bin)
{
    wgpu::SupportedLimits supportedLimits;
    device.GetLimits(&supportedLimits);
    wgpu::Limits deviceLimits = supportedLimits.limits;

    uniformStride = ceilToNextMultiple(
        (uint32_t)sizeof(ModelData),
        (uint32_t)deviceLimits.minUniformBufferOffsetAlignment
    );
    
    std::string fullpath = std::string(RESOURCE_DIR) + "/" + name;

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    bool ret;
    if (bin)
        ret = loader.LoadBinaryFromFile(&model, &err, &warn, fullpath);
    else
        ret = loader.LoadASCIIFromFile(&model, &err, &warn, fullpath);
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
    meshes.resize(model.meshes.size());
    LoadMeshes(model);
    LoadNodes(model);
    LoadMaterials(model);
    
    InitUniforms();
    InitModelBindgroups();
    InitTextureBindGroups(model);
    if(model.animations.size() > 0)
        LoadAnimations(model);

    LoadSkin(model);
    //model.materials[0].pbrMetallicRoughness.baseColorTexture.index

    startTime = std::chrono::high_resolution_clock::now();
    UpdateNodes();
    std::cout << "Succesfully loaded model" << std::endl;
}

void Model::LoadNodes(tinygltf::Model& m)
{
    int i = 0;
    int emptyNames = 0;
    for (tinygltf::Node& node : m.nodes)
    {
        Node* newNode = new Node();
        if (node.name.empty())
        {
            newNode->name = "Node." + std::to_string(emptyNames);
            emptyNames++;
        }
        else
            newNode->name = node.name;
        if (node.mesh == -1)
            newNode->mesh = nullptr;
        else
            newNode->mesh = &meshes[node.mesh];
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
            newNode->localRotation = glm::quat(static_cast<float>(node.rotation[3]), 
                     static_cast<float>(node.rotation[0]), 
                     static_cast<float>(node.rotation[1]),
                     static_cast<float>(node.rotation[2]));  
        }
        else
        {
            newNode->localRotation = {1,0,0,0};
        }

        glm::mat4x4 modelMatrix = glm::translate(glm::mat4(1.0f), newNode->localPosition)
         * glm::mat4_cast(newNode->localRotation)
         * glm::scale(glm::mat4(1.0f), newNode->localScale);
        newNode->modelMatrix = modelMatrix;
        nodes.push_back(newNode);
        if (newNode->mesh != nullptr) DrawableNodes.push_back(newNode);
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
    glm::mat4 zUpToYUpRotation = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat zUpToYUpQuat = glm::quat_cast(zUpToYUpRotation);  
    for (int i = 0; i < m.scenes[0].nodes.size(); i++)
    {
        Node* rootNode = nodes[m.scenes[0].nodes[i]];

        rootNode->modelMatrix *= zUpToYUpRotation;

        rootNode->localRotation = zUpToYUpQuat * rootNode->localRotation;
        rootNodes.push_back(rootNode);
    }
}

void Model::InitUniforms()
{
    using namespace wgpu;
    for (int i = 0; i < DrawableNodes.size(); i++)
    {
        for (int j = 0; j < DrawableNodes[i]->mesh->submeshes.size(); j++)
        {
            ModelData data;
            if (DrawableNodes[i]->mesh->submeshes[j].materialIndex == -1)
            {
                MaterialProperties newProps;
                materials.push_back(newProps);
                DrawableNodes[i]->mesh->submeshes[j].materialIndex = static_cast<int>(materials.size()) - 1;
            }
            data.material = materials[DrawableNodes[i]->mesh->submeshes[j].materialIndex];
            data.modelMatrix = DrawableNodes[i]->modelMatrix;
            //TODO: Init texture bindgroups for each modeldata
            modelData.push_back(data);
        }
    }

    SupportedLimits supportedLimits;
    device.GetLimits(&supportedLimits);
    Limits deviceLimits = supportedLimits.limits;

    uniformStride = ceilToNextMultiple(
        (uint32_t)sizeof(ModelData),
        (uint32_t)deviceLimits.minUniformBufferOffsetAlignment
    );

    BufferDescriptor bufferDesc;
    bufferDesc.size = uniformStride * (modelData.size() - 1) + sizeof(ModelData);
    bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Uniform;
    bufferDesc.mappedAtCreation = false;
    modelsBuffer = device.CreateBuffer(&bufferDesc);

    for (int i = 0; i < modelData.size(); i++)
        device.GetQueue().WriteBuffer(modelsBuffer, i * uniformStride, &modelData[i], sizeof(ModelData));
}

void Model::InitModelBindgroups()
{
    using namespace wgpu;
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
    modelDataBindGroup = device.CreateBindGroup(&bindGroupDesc);
}

wgpu::TextureView Model::GetTexture(tinygltf::Model& model, int index, wgpu::TextureFormat format)
{
    using namespace wgpu;
    if (textures.size() != model.images.size())
    {
        textures.resize(model.images.size(), std::nullopt);
    }
    //TODO: add saftey check that index != -1
    if (index == -1)
    {
        return {};
    }
    if (textures[index])
    {
        std::cout << "Using loaded texture" << std::endl;
        return textures[index].value();
    }
    tinygltf::Image img = model.images[index];
    int width = img.width;
    int height = img.height;
    int channels = img.component;
    unsigned char* imageData = img.image.data();

    if (imageData == nullptr) {
        std::cerr << "Failed to load texture from gltf model!" << std::endl;
        return {};
    }

    std::vector<uint8_t> pixels(channels * width * height);
    std::memcpy(pixels.data(), imageData, pixels.size());
    //Create Texture
    TextureFormat textureFormat = format;
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
    
    textures[index] = textureView;
    return textureView;
}

void Model::InitTextureBindGroups(tinygltf::Model& m)
{
    using namespace wgpu;
    int i = 0;
    for (MaterialTexturesData& materialTexture : materialTextures)
    {
        std::vector<BindGroupLayoutEntry> textureBindingLayouts(1);
        textureBindingLayouts[0] = {};
        textureBindingLayouts[0].binding = 0;
        textureBindingLayouts[0].visibility = ShaderStage::Fragment;
        textureBindingLayouts[0].texture.sampleType = TextureSampleType::Float;
        textureBindingLayouts[0].texture.viewDimension = TextureViewDimension::e2D;

        BindGroupLayoutDescriptor textureBindGroupLayoutDesc{};
        textureBindGroupLayoutDesc.entryCount = (uint32_t)textureBindingLayouts.size();
        textureBindGroupLayoutDesc.entries = textureBindingLayouts.data();
        wgpu::BindGroupLayout textureBindGroupLayout = device.CreateBindGroupLayout(&textureBindGroupLayoutDesc);

        std::vector<BindGroupEntry> textureBindings(1);
        textureBindings[0] = {};
        textureBindings[0].binding = 0;
        textureBindings[0].textureView = GetTexture(m, materialTexture.albedoTexture, TextureFormat::RGBA8UnormSrgb);

        BindGroupDescriptor bindGroupDesc{};
        bindGroupDesc.layout = textureBindGroupLayout;
        bindGroupDesc.entryCount = (uint32_t)textureBindings.size();
        bindGroupDesc.entries = textureBindings.data();
        textreDataBindGroups.push_back(device.CreateBindGroup(&bindGroupDesc));
        i++;
    }
}

void Model::TraverseNodes(Node* node, glm::mat4x4 parentMatrix)
{
    node->modelMatrix = parentMatrix * glm::translate(glm::mat4(1.0f), node->localPosition)
         * glm::mat4_cast(node->localRotation)
         * glm::scale(glm::mat4(1.0f), node->localScale);
    for (Node* n : node->children)
        TraverseNodes(n, node->modelMatrix);
}

void Model::UpdateAnimatedNodes()
{
    if(animations.size() == 0)
        return;
    auto start = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> elapsed = std::chrono::high_resolution_clock::now() - startTime;
    float time = std::fmod(elapsed.count(), animationLength);
    for (AnimationChannel channel : animations[0].channels)
    {
        if(channel.targetNodeIndex == -1) 
            continue;
        if (channel.type == AnimationChannelType::TRANSLATION)
        {
            glm::vec3 newPosition = channel.InterpolatePosition(time);
            nodes[channel.targetNodeIndex]->localPosition = newPosition;
        }
        else if (channel.type == AnimationChannelType::SCALE)
        {
            glm::vec3 newScale = channel.InterpolatePosition(time);
            nodes[channel.targetNodeIndex]->localScale = newScale;
        }
        else if (channel.type == AnimationChannelType::ROTATION)
        {
            glm::quat newRot = channel.InterpolateRotation(time);
            nodes[channel.targetNodeIndex]->localRotation = newRot;
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    //std::cout << "exampleFunction took " << duration.count() << " microseconds." << std::endl;
}

void Model::UpdateNodes()
{
    UpdateAnimatedNodes();
    for (Node* n : rootNodes)
        TraverseNodes(n, glm::mat4x4(1.0f));
    int index = 0;
    for (int i = 0; i < DrawableNodes.size(); i++)
    {
        for (int j = 0; j < DrawableNodes[i]->mesh->submeshes.size(); j++)
        {
            modelData[index].modelMatrix = DrawableNodes[i]->modelMatrix;
            index++;
        }
    }
    for (int i = 0; i < modelData.size(); i++)
        device.GetQueue().WriteBuffer(modelsBuffer, uniformStride * i, &modelData[i], sizeof(ModelData));

    FixJointMatrices();
}

void Model::LoadMaterials(tinygltf::Model& m)
{
    for(tinygltf::Material& mat : m.materials) 
    {
        MaterialProperties newMaterial;
        MaterialTexturesData textureData;
        newMaterial.textureFlags = 0;
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

        if(mat.pbrMetallicRoughness.baseColorTexture.index != -1)
        {
            newMaterial.textureFlags |= (1 << 0);
        }
        textureData.albedoTexture = mat.pbrMetallicRoughness.baseColorTexture.index;
        materialTextures.push_back(textureData);
        materials.push_back(newMaterial);
    }
}

void Model::LoadMeshes(tinygltf::Model& model)
{
    for (int i = 0; i < model.meshes.size(); i++)
    {
        std::vector<Vertex> vertexData;
        std::vector<SkinnedVertex> skinnedVertexData;
        std::vector<uint16_t> indices;
        std::vector<Submesh> subs;
        for (const auto& primitive : model.meshes[i].primitives) {
            Submesh subMesh;
            subMesh.materialIndex = primitive.material;
            subMesh.startIndex = indices.size();
            subMesh.startVertex = vertexData.size();
            std::vector<glm::vec3> positions;
            if (primitive.attributes.find("POSITION") != primitive.attributes.end()) {
                int posAccessorIndex = primitive.attributes.at("POSITION");
                const tinygltf::Accessor& posAccessor = model.accessors[posAccessorIndex];
                const tinygltf::BufferView& posBufferView = model.bufferViews[posAccessor.bufferView];
                const tinygltf::Buffer& posBuffer = model.buffers[posBufferView.buffer];

                const unsigned char* bufferStart = posBuffer.data.data() + posBufferView.byteOffset;
                const unsigned char* accessorStart = bufferStart + posAccessor.byteOffset;

                size_t stride = posAccessor.ByteStride(posBufferView);
                if (stride == 0) {
                    stride = 3 * sizeof(float); // Default stride for vec3 (tightly packed)
                }

                size_t numVertices = posAccessor.count;
                positions.reserve(numVertices);

                for (size_t i = 0; i < numVertices; ++i) {
                    const float* posData = reinterpret_cast<const float*>(accessorStart + i * stride);
                    positions.push_back(glm::vec3(posData[0], posData[1], posData[2]));
                }
            }

            std::cout << "Loading positions" << std::endl;

            std::vector<glm::vec3> normals;
            if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
                int normalAccessorIndex = primitive.attributes.at("NORMAL");
                const tinygltf::Accessor& normalAccessor = model.accessors[normalAccessorIndex];
                const tinygltf::BufferView& normalBufferView = model.bufferViews[normalAccessor.bufferView];
                const tinygltf::Buffer& normalBuffer = model.buffers[normalBufferView.buffer];

                const unsigned char* bufferStart = normalBuffer.data.data() + normalBufferView.byteOffset;
                const unsigned char* accessorStart = bufferStart + normalAccessor.byteOffset;

                size_t stride = normalAccessor.ByteStride(normalBufferView);
                if (stride == 0) {
                    stride = 3 * sizeof(float); // Default stride for vec3 (tightly packed)
                }

                size_t numNormals = normalAccessor.count;
                normals.reserve(numNormals);

                for (size_t i = 0; i < numNormals; ++i) {
                    const float* normalData = reinterpret_cast<const float*>(accessorStart + i * stride);
                    normals.push_back(glm::vec3(normalData[0], normalData[1], normalData[2]));
                }
            }

            std::cout << "Loading normals" << std::endl;

            std::vector<glm::vec2> uvs;
            if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
                int uvAccessorIndex = primitive.attributes.at("TEXCOORD_0");
                const tinygltf::Accessor& uvAccessor = model.accessors[uvAccessorIndex];
                const tinygltf::BufferView& uvBufferView = model.bufferViews[uvAccessor.bufferView];
                const tinygltf::Buffer& uvBuffer = model.buffers[uvBufferView.buffer];

                const unsigned char* bufferStart = uvBuffer.data.data() + uvBufferView.byteOffset;
                const unsigned char* accessorStart = bufferStart + uvAccessor.byteOffset;

                size_t stride = uvAccessor.ByteStride(uvBufferView);
                if (stride == 0) {
                    stride = 2 * sizeof(float);
                }

                size_t numUVs = uvAccessor.count;
                uvs.reserve(numUVs);

                for (size_t i = 0; i < numUVs; ++i) {
                    const float* uvData = reinterpret_cast<const float*>(accessorStart + i * stride);
                    uvs.push_back(glm::vec2(uvData[0], uvData[1]));
                }
            }

            std::cout << "Loading uvs" << std::endl;

            std::vector<glm::vec4> colors;
            if (primitive.attributes.find("COLOR_0") != primitive.attributes.end()) {
                int colorAccessorIndex = primitive.attributes.at("COLOR_0");
                const tinygltf::Accessor& colorAccessor = model.accessors[colorAccessorIndex];
                const tinygltf::BufferView& colorBufferView = model.bufferViews[colorAccessor.bufferView];
                const tinygltf::Buffer& colorBuffer = model.buffers[colorBufferView.buffer];

                const unsigned char* bufferStart = colorBuffer.data.data() + colorBufferView.byteOffset;
                const unsigned char* accessorStart = bufferStart + colorAccessor.byteOffset;

                size_t stride = colorAccessor.ByteStride(colorBufferView);
                if (stride == 0) {
                    stride = 4 * sizeof(float); // Default stride for vec4 (tightly packed)
                }

                size_t numColors = colorAccessor.count;
                colors.reserve(numColors);

                for (size_t i = 0; i < numColors; ++i) {
                    const float* colorData = reinterpret_cast<const float*>(accessorStart + i * stride);
                    colors.push_back(glm::vec4(colorData[0], colorData[1], colorData[2], colorData[3]));
                }
            }

            std::cout << "Loading colors" << std::endl;
            size_t numVertices = positions.size();
            std::vector<glm::ivec4> boneIndices;
            if (primitive.attributes.find("JOINTS_0") != primitive.attributes.end()) {
                int jointsAccessorIndex = primitive.attributes.at("JOINTS_0");
                const tinygltf::Accessor& jointsAccessor = model.accessors[jointsAccessorIndex];
                const tinygltf::BufferView& jointsBufferView = model.bufferViews[jointsAccessor.bufferView];
                const tinygltf::Buffer& jointsBuffer = model.buffers[jointsBufferView.buffer];

                const unsigned char* bufferStart = jointsBuffer.data.data() + jointsBufferView.byteOffset;
                const unsigned char* accessorStart = bufferStart + jointsAccessor.byteOffset;

                size_t stride = jointsAccessor.ByteStride(jointsBufferView);
                if (stride == 0) {
                    stride = 4 * sizeof(uint16_t); // Default stride for vec4 (tightly packed)
                }

                size_t numJoints = jointsAccessor.count;

                for (size_t i = 0; i < numJoints; ++i) {
                    glm::ivec4 jointIndices(0); // Initialize to zero

                    if (jointsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                        const uint8_t* jointsData = reinterpret_cast<const uint8_t*>(accessorStart + i * stride);
                        jointIndices = glm::ivec4(jointsData[0], jointsData[1], jointsData[2], jointsData[3]);
                    } else if (jointsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                        const uint16_t* jointsData = reinterpret_cast<const uint16_t*>(accessorStart + i * stride);
                        jointIndices = glm::ivec4(jointsData[0], jointsData[1], jointsData[2], jointsData[3]);
                    } else {
                        std::cerr << "Unsupported JOINTS_0 component type: " << jointsAccessor.componentType << std::endl;
                    }

                    boneIndices.push_back(jointIndices);
                }
            }
            else
            {
                for (size_t i = 0; i < positions.size(); ++i) {
                    boneIndices.push_back(glm::ivec4(-1,0,0,0));
                }
            }

            std::vector<glm::vec4> boneWeights;
            if (primitive.attributes.find("WEIGHTS_0") != primitive.attributes.end()) {
                int weightsAccessorIndex = primitive.attributes.at("WEIGHTS_0");
                const tinygltf::Accessor& weightsAccessor = model.accessors[weightsAccessorIndex];
                const tinygltf::BufferView& weightsBufferView = model.bufferViews[weightsAccessor.bufferView];
                const tinygltf::Buffer& weightsBuffer = model.buffers[weightsBufferView.buffer];

                const unsigned char* bufferStart = weightsBuffer.data.data() + weightsBufferView.byteOffset;
                const unsigned char* accessorStart = bufferStart + weightsAccessor.byteOffset;

                size_t stride = weightsAccessor.ByteStride(weightsBufferView);
                if (stride == 0) {
                    stride = 4 * sizeof(float); // Default stride for vec4 (tightly packed)
                }

                size_t numWeights = weightsAccessor.count;

                for (size_t i = 0; i < numWeights; ++i) {
                    const float* weights = reinterpret_cast<const float*>(accessorStart + i * stride);
                    boneWeights.push_back(glm::vec4(weights[0], weights[1], weights[2], weights[3]));
                }
            }
            else
            {
                for (size_t i = 0; i < positions.size(); ++i) {
                    boneWeights.push_back(glm::vec4(0,0,0,0));
                }
            }

            std::cout << "Loading bone weights" << std::endl;

            
            // if (normals.size() != numVertices/* || uvs.size() != numVertices*/) {
            //     std::cerr << "Error: Mismatch in number of positions, normals, or UVs\n";
            //     continue;
            // }
            // else
            //     std::cout << "Mesh created succesfully" << std::endl;

            for (size_t i = 0; i < numVertices; ++i) {
                Vertex v = {};
                v.position = positions[i];
                if(normals.size() > 0)
                    v.normal = normals[i];
                if(colors.size() > 0)
                    v.color = colors[i];
                if(uvs.size() > 0)
                    v.uv = uvs[i];
                vertexData.push_back(v);

                SkinnedVertex sk = {};
                sk.indices = boneIndices[i];
                sk.weights = boneWeights[i];

                skinnedVertexData.push_back(sk);
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
                        indices.push_back(indicesData[i] + subMesh.startVertex);
                    }
                } else {
                    std::cerr << "Unsupported index component type: " << indicesAccessor.componentType << std::endl;
                }
            }  
            else std::cout << "WHAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA" << std::endl; 
            subMesh.indexxCount = indices.size() - subMesh.startIndex;
            subMesh.vertexCount = vertexData.size() - subMesh.startVertex;
            subs.push_back(subMesh);
        }
        wgpu::BufferDescriptor bufferDesc;
        bufferDesc.size = vertexData.size() * sizeof(Vertex);
        bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex;
        bufferDesc.mappedAtCreation = false;
        meshes[i].vertexBuffer = device.CreateBuffer(&bufferDesc);
        device.GetQueue().WriteBuffer(meshes[i].vertexBuffer, 0, vertexData.data(), bufferDesc.size);

        bufferDesc.size = skinnedVertexData.size() * sizeof(SkinnedVertex);
        bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex;
        bufferDesc.mappedAtCreation = false;
        meshes[i].skinnedVertexBuffer = device.CreateBuffer(&bufferDesc);
        device.GetQueue().WriteBuffer(meshes[i].skinnedVertexBuffer, 0, skinnedVertexData.data(), bufferDesc.size);

        if (skinnedVertexData.size() == vertexData.size())
        {
            std::cout << "WEYYY skinned data nice" << std::endl;
        } else std::cout << "BOOOOOO skinned data bad" << std::endl;

        bufferDesc.size = indices.size() * sizeof(uint16_t);
        bufferDesc.size = (bufferDesc.size + 3) & ~3;
        bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Index;

        meshes[i].indexBuffer = device.CreateBuffer(&bufferDesc);
        device.GetQueue().WriteBuffer(meshes[i].indexBuffer, 0, indices.data(), bufferDesc.size); 
        meshes[i].indexCount = indices.size();
        meshes[i].submeshes = subs;
    }
}

void Model::LoadAnimations(tinygltf::Model& m)
{
    if (m.animations.size() == 0) {
        std::cerr << "No animations found in the model." << std::endl;
        return;
    }
    AnimationData animationData;
    animationData.name = m.animations[0].name;
    std::cout << "Loading animation: " << animationData.name << std::endl;
    // Load the first animation
    for (const tinygltf::Animation& animation : m.animations)
    {
        if (animation.channels.size() == 0) {
            std::cerr << "No channels in the first animation." << std::endl;
            return;
        }

        // Assume the first channel targets translation
        for (const tinygltf::AnimationChannel& channel : animation.channels)
        {
            AnimationChannel animationChannel;

            if (channel.target_path == "translation")
            {
                animationChannel.type = AnimationChannelType::TRANSLATION;
            }
            else if (channel.target_path == "rotation")
            {
                animationChannel.type = AnimationChannelType::ROTATION;
            }
            else if (channel.target_path == "scale")
            {
                animationChannel.type = AnimationChannelType::SCALE;
            }
            else if (channel.target_path == "weights")
            {
                std::cerr << "The channel was for weights?????" << std::endl;
                continue; // Skip this iteration
            }

            animationChannel.targetNodeIndex = channel.target_node;

            int samplerIndex = channel.sampler;
            if (samplerIndex < 0 || samplerIndex >= animation.samplers.size()) {
                std::cerr << "Invalid sampler index." << std::endl;
                return;
            }
            
            const tinygltf::AnimationSampler& sampler = animation.samplers[samplerIndex];

            const std::string& interpolation = sampler.interpolation;
            if (interpolation == "LINEAR") {
                animationChannel.interpolationMode = AnimationInterpolationType::LINEAR;
            } else if (interpolation == "STEP") {
                animationChannel.interpolationMode = AnimationInterpolationType::STEP;
            } else if (interpolation == "CUBICSPLINE") {
                animationChannel.interpolationMode = AnimationInterpolationType::SPLINE;
            }

            // Access input accessor (keyframe times)
            const tinygltf::Accessor& inputAccessor = m.accessors[sampler.input];
            const tinygltf::BufferView& inputBufferView = m.bufferViews[inputAccessor.bufferView];
            const tinygltf::Buffer& inputBuffer = m.buffers[inputBufferView.buffer];

            std::vector<float> keyframeTimes(inputAccessor.count);
            animationChannel.keyFrames.resize(inputAccessor.count);
            memcpy(
                keyframeTimes.data(),
                inputBuffer.data.data() + inputBufferView.byteOffset + inputAccessor.byteOffset,
                inputAccessor.count * sizeof(float)
            );

            // Access output accessor (keyframe values, vec3 for translation)
            const tinygltf::Accessor& outputAccessor = m.accessors[sampler.output];
            const tinygltf::BufferView& outputBufferView = m.bufferViews[outputAccessor.bufferView];
            const tinygltf::Buffer& outputBuffer = m.buffers[outputBufferView.buffer];

            if (outputAccessor.type != TINYGLTF_TYPE_VEC3 && outputAccessor.type != TINYGLTF_TYPE_VEC4) {
                std::cerr << "Output accessor does not contain vec3 or vec4 data." << std::endl;
                return;
            }

            size_t numKeyframes = outputAccessor.count;


            if (animationChannel.type == AnimationChannelType::ROTATION) {
                if (animationChannel.interpolationMode == AnimationInterpolationType::LINEAR || 
                    animationChannel.interpolationMode == AnimationInterpolationType::STEP) {
                    // Handle LINEAR and STEP as before
                    std::vector<std::array<float, 4>> keyframeValues(numKeyframes);
                    memcpy(
                        keyframeValues.data(),
                        outputBuffer.data.data() + outputBufferView.byteOffset + outputAccessor.byteOffset,
                        numKeyframes * sizeof(std::array<float, 4>)
                    );
                    for (int i = 0; i < keyframeTimes.size(); i++) {
                        animationChannel.keyFrames[i].time = keyframeTimes[i];
                        if (animationChannel.keyFrames[i].time > animationLength)
                            animationLength = animationChannel.keyFrames[i].time;
                        animationChannel.keyFrames[i].data.assign(keyframeValues[i].begin(), keyframeValues[i].end());
                    }
                } else if (animationChannel.interpolationMode == AnimationInterpolationType::SPLINE) {
                    // Handle CUBICSPLINE
                    std::vector<std::array<float, 4>> inTangents(numKeyframes);
                    std::vector<std::array<float, 4>> keyframeValues(numKeyframes);
                    std::vector<std::array<float, 4>> outTangents(numKeyframes);

                    size_t stride = sizeof(std::array<float, 4>); // Assume each component is VEC4

                    const uint8_t* basePtr = outputBuffer.data.data() + outputBufferView.byteOffset + outputAccessor.byteOffset;

                    for (size_t i = 0; i < numKeyframes; ++i) {
                        // Read in-tangent
                        memcpy(inTangents[i].data(), basePtr + i * stride * 3, stride);

                        // Read keyframe value
                        memcpy(keyframeValues[i].data(), basePtr + i * stride * 3 + stride, stride);

                        // Read out-tangent
                        memcpy(outTangents[i].data(), basePtr + i * stride * 3 + stride * 2, stride);
                    }

                    for (int i = 0; i < keyframeTimes.size(); i++) {
                        animationChannel.keyFrames[i].time = keyframeTimes[i];
                        if (animationChannel.keyFrames[i].time > animationLength)
                            animationLength = animationChannel.keyFrames[i].time;

                        // Assign keyframe data
                        animationChannel.keyFrames[i].data.assign(keyframeValues[i].begin(), keyframeValues[i].end());

                        // Assign tangents (optional, depending on your structure)
                        animationChannel.keyFrames[i].inTangent.assign(inTangents[i].begin(), inTangents[i].end());
                        animationChannel.keyFrames[i].outTangent.assign(outTangents[i].begin(), outTangents[i].end());
                    }
                } else {
                    throw std::runtime_error("Unsupported interpolation mode.");
                }
            } else if (animationChannel.type == AnimationChannelType::TRANSLATION || animationChannel.type == AnimationChannelType::SCALE) {
                if (animationChannel.interpolationMode == AnimationInterpolationType::LINEAR || 
                    animationChannel.interpolationMode == AnimationInterpolationType::STEP) {
                    // Handle LINEAR and STEP
                    std::vector<std::array<float, 3>> keyframeValues(numKeyframes);
                    memcpy(
                        keyframeValues.data(),
                        outputBuffer.data.data() + outputBufferView.byteOffset + outputAccessor.byteOffset,
                        numKeyframes * sizeof(std::array<float, 3>)
                    );

                    for (int i = 0; i < keyframeTimes.size(); i++) {
                        animationChannel.keyFrames[i].time = keyframeTimes[i];
                        if (animationChannel.keyFrames[i].time > animationLength)
                            animationLength = animationChannel.keyFrames[i].time;

                        animationChannel.keyFrames[i].data.assign(keyframeValues[i].begin(), keyframeValues[i].end());
                    }
                } else if (animationChannel.interpolationMode == AnimationInterpolationType::SPLINE) {
                    // Handle CUBICSPLINE
                    std::vector<std::array<float, 3>> inTangents(numKeyframes);
                    std::vector<std::array<float, 3>> keyframeValues(numKeyframes);
                    std::vector<std::array<float, 3>> outTangents(numKeyframes);

                    size_t stride = sizeof(std::array<float, 3>); // Assume each component is VEC3

                    const uint8_t* basePtr = outputBuffer.data.data() + outputBufferView.byteOffset + outputAccessor.byteOffset;

                    for (size_t i = 0; i < numKeyframes; ++i) {
                        // Read in-tangent
                        memcpy(inTangents[i].data(), basePtr + i * stride * 3, stride);

                        // Read keyframe value
                        memcpy(keyframeValues[i].data(), basePtr + i * stride * 3 + stride, stride);

                        // Read out-tangent
                        memcpy(outTangents[i].data(), basePtr + i * stride * 3 + stride * 2, stride);
                    }

                    for (int i = 0; i < keyframeTimes.size(); i++) {
                        animationChannel.keyFrames[i].time = keyframeTimes[i];
                        if (animationChannel.keyFrames[i].time > animationLength)
                            animationLength = animationChannel.keyFrames[i].time;

                        // Assign keyframe data
                        animationChannel.keyFrames[i].data.assign(keyframeValues[i].begin(), keyframeValues[i].end());

                        // Assign tangents (optional, depending on your structure)
                        animationChannel.keyFrames[i].inTangent.assign(inTangents[i].begin(), inTangents[i].end());
                        animationChannel.keyFrames[i].outTangent.assign(outTangents[i].begin(), outTangents[i].end());
                    }
                } else {
                    throw std::runtime_error("Unsupported interpolation mode.");
                }
            }

            
            animationData.channels.push_back(animationChannel);
            }
        }
    animations.push_back(animationData);
}

void Model::LoadSkin(tinygltf::Model& model)
{
    if (model.skins.size() != 0)
    {
        const tinygltf::Skin& skin = model.skins[0];
        joints = skin.joints;

        const tinygltf::Accessor& inverseBindAccessor = model.accessors[skin.inverseBindMatrices];
        const tinygltf::BufferView& inverseBindBufferView = model.bufferViews[inverseBindAccessor.bufferView];
        const tinygltf::Buffer& inverseBindBuffer = model.buffers[inverseBindBufferView.buffer];

        inverseBindMatrices.resize(inverseBindAccessor.count);
        const float* inverseBindData = reinterpret_cast<const float*>(
            &inverseBindBuffer.data[inverseBindBufferView.byteOffset + inverseBindAccessor.byteOffset]);

        for (size_t i = 0; i < inverseBindAccessor.count; ++i) {
            glm::mat4 mat;
            std::memcpy(glm::value_ptr(mat), &inverseBindData[i * 16], sizeof(glm::mat4));
            inverseBindMatrices[i] = mat;
        }
    }
    else
        joints.push_back(-1);

    jointMatrices.resize(joints.size(), glm::mat4(1.0f));

    size_t jointCount = joints.size();
    wgpu::BufferDescriptor boneBufferDesc{};
    boneBufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    boneBufferDesc.size = jointCount * sizeof(glm::mat4x4);
    boneBufferDesc.mappedAtCreation = false;
    boneBuffer = device.CreateBuffer(&boneBufferDesc);

    std::vector<wgpu::BindGroupLayoutEntry> boneBindingLayouts(1);
    boneBindingLayouts[0] = {};
    boneBindingLayouts[0].binding = 0;
    boneBindingLayouts[0].visibility = wgpu::ShaderStage::Vertex;
    boneBindingLayouts[0].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
    boneBindingLayouts[0].buffer.hasDynamicOffset = false; 
    boneBindingLayouts[0].buffer.minBindingSize = 0; 

    wgpu::BindGroupLayoutDescriptor boneBindGroupLayoutDesc{};
    boneBindGroupLayoutDesc.entryCount = (uint32_t)boneBindingLayouts.size();
    boneBindGroupLayoutDesc.entries = boneBindingLayouts.data();
    wgpu::BindGroupLayout boneBindGroupLayout = device.CreateBindGroupLayout(&boneBindGroupLayoutDesc);



    wgpu::BindGroupEntry boneEntry{};
    boneEntry.binding = 0;
    boneEntry.buffer = boneBuffer;
    boneEntry.offset = 0;
    boneEntry.size = jointMatrices.size() * sizeof(glm::mat4);

    wgpu::BindGroupDescriptor boneBindGroupDesc{};
    boneBindGroupDesc.layout = boneBindGroupLayout;
    boneBindGroupDesc.entryCount = 1;
    boneBindGroupDesc.entries = &boneEntry;

    boneBindGroup = device.CreateBindGroup(&boneBindGroupDesc);
}

void Model::FixJointMatrices()
{       
    size_t jointCount = joints.size();
    for (size_t i = 0; i < jointCount; ++i) {
        if(joints[i] == -1)
            continue;
        int jointNodeIndex = joints[i];
        jointMatrices[i] = nodes[jointNodeIndex]->modelMatrix * inverseBindMatrices[i];
    }
    
    

    device.GetQueue().WriteBuffer(
        boneBuffer,  
        0,           
        jointMatrices.data(), 
        jointCount * sizeof(glm::mat4)  
    );
}

void Model::Draw(wgpu::RenderPassEncoder& renderPass)
{
    UpdateNodes();

    int index = 0;
    for (Node* n : DrawableNodes)
    {
        for (Submesh& s : n->mesh->submeshes)
        {
            uint32_t dynamicOffset = index * uniformStride;
            renderPass.SetVertexBuffer(0, n->mesh->vertexBuffer, 0, n->mesh->vertexBuffer.GetSize());
            renderPass.SetVertexBuffer(1, n->mesh->skinnedVertexBuffer, 0, n->mesh->skinnedVertexBuffer.GetSize());
            renderPass.SetIndexBuffer(n->mesh->indexBuffer, wgpu::IndexFormat::Uint16,  s.startIndex * sizeof(uint16_t), s.indexxCount * sizeof(uint16_t));
            renderPass.SetBindGroup(1, modelDataBindGroup, 1, &dynamicOffset);
            renderPass.SetBindGroup(2, boneBindGroup, 0, nullptr); 
            renderPass.SetBindGroup(3, textreDataBindGroups[s.materialIndex], 0, nullptr);
            renderPass.DrawIndexed(s.indexxCount, 1, 0, 0);
            index++;
        }
    }
}

Model::~Model()
{
    for (Node* n : nodes)
        delete n;
    std::cout << "Unloading model with name " << rootNodes[0]->name << std::endl;
}