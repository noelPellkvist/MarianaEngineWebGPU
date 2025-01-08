#include "Model.hpp"
#include <iostream>

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
    LoadNodes(model);
    LoadMaterials(model);
    LoadMeshes(model);
    InitUniforms();
    InitModelBindgroups();
    if(model.animations.size() > 0)
        LoadAnimations(model);

    startTime = std::chrono::high_resolution_clock::now();
    UpdateAnimatedNodes();
    
    std::cout << "Succesfully loaded model" << std::endl;
}

void Model::LoadNodes(tinygltf::Model& m)
{
    int i = 0;
    for (tinygltf::Node& node : m.nodes)
    {
        Node* newNode = new Node();
        newNode->name = node.name;
        if(node.mesh != -1)
        {
            MeshData newMeshData;
            newMeshData.nodeIndex = i;
            meshes[node.mesh] = newMeshData;
        }
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

    for (int i = 0; i < m.scenes[0].nodes.size(); i++)
    {
        rootNodes.push_back(nodes[m.scenes[0].nodes[i]]);
    }
}

void Model::InitUniforms()
{
    using namespace wgpu;
    for (int i = 0; i < subMeshes.size(); i++)
    {
        ModelData data;
        if (subMeshes[i].materialIndex == -1)
        {
            MaterialProperties newProps;
            materials.push_back(newProps);
            subMeshes[i].materialIndex = static_cast<int>(materials.size()) - 1;
        }
        data.material = materials[subMeshes[i].materialIndex];
        data.modelMatrix = nodes[subMeshes[i].nodeIndex]->modelMatrix;
        modelData.push_back(data);
    }

    SupportedLimits supportedLimits;
    device.GetLimits(&supportedLimits);
    Limits deviceLimits = supportedLimits.limits;

    uniformStride = ceilToNextMultiple(
        (uint32_t)sizeof(ModelData),
        (uint32_t)deviceLimits.minUniformBufferOffsetAlignment
    );

    BufferDescriptor bufferDesc;
    bufferDesc.size = uniformStride * (subMeshes.size() - 1) + sizeof(ModelData);
    bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Uniform;
    bufferDesc.mappedAtCreation = false;
    modelsBuffer = device.CreateBuffer(&bufferDesc);

    for (int i = 0; i < subMeshes.size(); i++)
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

void Model::InitTextureBindgroups()
{
    using namespace wgpu;
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


    for(int i = 0; i < subMeshes.size(); i++)
    {
        std::vector<BindGroupEntry> bindings(1);
        bindings[0] = {};
        bindings[0].binding = 0;


        BindGroupDescriptor bindGroupDesc{};
        bindGroupDesc.layout = textureBindGroupLayout;
        bindGroupDesc.entryCount = bindings.size();
        bindGroupDesc.entries = bindings.data();
        wgpu::BindGroup textureBindGroup = device.CreateBindGroup(&bindGroupDesc);
        TextureBindings.push_back(textureBindGroup);
    }
}

void Model::TraverseNodes(Node* node, glm::mat4x4 parentMatrix)
{
    node->modelMatrix = parentMatrix * glm::translate(glm::mat4(1.0f), node->localPosition)
         * glm::mat4_cast(node->localRotation)
         * glm::scale(glm::mat4(1.0f), node->localScale);
    for (Node* n : node->children)
        TraverseNodes(n, node->modelMatrix);
    std::cout << std::endl;
}

void Model::UpdateAnimatedNodes()
{
    if(animations.size() == 0)
        return;
    std::chrono::duration<float> elapsed = std::chrono::high_resolution_clock::now() - startTime;
    float time = std::fmod(elapsed.count(), 2.5f);
    for (AnimationChannel channel : animations[0].channels)
    {
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
    
    //std::cout << "Position: " << newPosition.x << ", " << newPosition.y << ", " << newPosition.z << std::endl;
}

void Model::UpdateNodes()
{
    UpdateAnimatedNodes();
    for (Node* n : rootNodes)
        TraverseNodes(n, glm::mat4x4(1.0f));
    for (int i = 0; i < subMeshes.size(); i++)
    {
        modelData[i].material = materials[subMeshes[i].materialIndex];
        modelData[i].modelMatrix = nodes[subMeshes[i].nodeIndex]->modelMatrix;
    }
    for (int i = 0; i < subMeshes.size(); i++)
        device.GetQueue().WriteBuffer(modelsBuffer, uniformStride * i, &modelData[i], sizeof(ModelData));
}

void Model::LoadMaterials(tinygltf::Model& m)
{
    for(tinygltf::Material& mat : m.materials) 
    {
        MaterialProperties newMaterial;
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

        materials.push_back(newMaterial);
    }
}

void Model::LoadMeshes(tinygltf::Model& model)
{
    for (int i = 0; i < model.meshes.size(); i++)
    {
        std::vector<Vertex> vertexData;
        std::vector<uint16_t> indices;
        for (const auto& primitive : model.meshes[i].primitives) {
            Submesh subMesh;
            subMesh.materialIndex = primitive.material;
            subMesh.nodeIndex = meshes[i].nodeIndex;
            subMesh.meshIndex = i;
            subMesh.startIndex = indices.size();
            subMesh.startVertex = vertexData.size();
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
            subMeshes.push_back(subMesh);
        }
        wgpu::BufferDescriptor bufferDesc;
        bufferDesc.size = vertexData.size() * sizeof(Vertex);
        bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex;
        bufferDesc.mappedAtCreation = false;
        meshes[i].vertexBuffer = device.CreateBuffer(&bufferDesc);
        device.GetQueue().WriteBuffer(meshes[i].vertexBuffer, 0, vertexData.data(), bufferDesc.size);

        bufferDesc.size = indices.size() * sizeof(uint16_t);
        bufferDesc.size = (bufferDesc.size + 3) & ~3;
        bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Index;

        meshes[i].indexBuffer = device.CreateBuffer(&bufferDesc);
        device.GetQueue().WriteBuffer(meshes[i].indexBuffer, 0, indices.data(), bufferDesc.size); 
        meshes[i].indexCount = indices.size();
    }
}

void Model::LoadAnimations(tinygltf::Model& m)
{
    if (m.animations.size() == 0()) {
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
                std::cerr << "Output accessor does not contain vec3 data." << std::endl;
                return;
            }

            size_t numKeyframes = outputAccessor.count;


            if(animationChannel.type == AnimationChannelType::ROTATION)
            {
                std::vector<std::array<float, 4>> keyframeValues(numKeyframes);
                memcpy(
                    keyframeValues.data(),
                    outputBuffer.data.data() + outputBufferView.byteOffset + outputAccessor.byteOffset,
                    numKeyframes * sizeof(std::array<float, 4>)
                );
                for(int i = 0; i < keyframeTimes.size(); i++)
                {
                    animationChannel.keyFrames[i].time = keyframeTimes[i];
                    animationChannel.keyFrames[i].data.assign(keyframeValues[i].begin(), keyframeValues[i].end());
                }
            }
            else if (animationChannel.type == AnimationChannelType::TRANSLATION || animationChannel.type == AnimationChannelType::SCALE)
            {
                std::vector<std::array<float, 3>> keyframeValues(numKeyframes);
                memcpy(
                    keyframeValues.data(),
                    outputBuffer.data.data() + outputBufferView.byteOffset + outputAccessor.byteOffset,
                    numKeyframes * sizeof(std::array<float, 3>)
                );
                for(int i = 0; i < keyframeTimes.size(); i++)
                {
                    animationChannel.keyFrames[i].time = keyframeTimes[i];
                    animationChannel.keyFrames[i].data.assign(keyframeValues[i].begin(), keyframeValues[i].end());
                }
            }
            animationData.channels.push_back(animationChannel);
            }
        }
    animations.push_back(animationData);
}

void Model::Draw(wgpu::RenderPassEncoder& renderPass)
{
    UpdateNodes();
    for(int i = 0; i < subMeshes.size(); i++)
    {
        uint32_t dynamicOffset = i * uniformStride;
        renderPass.SetVertexBuffer(0, meshes[subMeshes[i].meshIndex].vertexBuffer, 0, meshes[subMeshes[i].meshIndex].vertexBuffer.GetSize());
        renderPass.SetIndexBuffer(meshes[subMeshes[i].meshIndex].indexBuffer, wgpu::IndexFormat::Uint16,  subMeshes[i].startIndex * sizeof(uint16_t), subMeshes[i].indexxCount * sizeof(uint16_t));
        renderPass.SetBindGroup(1, modelDataBindGroup, 1, &dynamicOffset);
        renderPass.DrawIndexed(subMeshes[i].indexxCount, 1, 0, 0);
    }
}

Model::~Model()
{
    for (Node* n : nodes)
        delete n;
    std::cout << "Unloading model with name " << rootNodes[0]->name << std::endl;
}