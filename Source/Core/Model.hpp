#pragma once

#include <vector>
#include <string>
#include <webgpu/webgpu_cpp.h>
#include <glm.hpp>
#include "GlobalVaribles.hpp"
#include "GameObject.hpp"


#include "../External/tiny_gltf.h"

struct TextureProperties
{
    uint32_t bindingIndex = -1;
    wgpu::TextureView textureView;
};

struct Submesh 
{
    uint32_t VertexStartIndex = 0;
    uint32_t IndexStartIndex = 0;
    uint32_t VertexCount = 0;
    uint32_t IndexCount = 0;
    MaterialProperties materialProps;
};

struct DrawData
{
    ModelData* data;
    NodesMesh* mesh;
    Node* node;
};

class Model {
    public:
        Model(std::string name);
        ~Model();

        void Draw(wgpu::RenderPassEncoder& renderPass);

        GameObject gameObject;
        Node* rootNode;

    private:
        std::vector<ModelData> LoadedModels;
        std::vector<TextureProperties> textures;
        std::vector<wgpu::BindGroupEntry> bindings;
        std::vector<NodesMesh> meshes;
        std::vector<DrawData> renderData;
        
        wgpu::BindGroup bindGroup;
        wgpu::Buffer modelsBuffer;

        uint32_t uniformStride;

        //void LoadMaterial(tinygltf::Material& mat);
        void LoadTexture(tinygltf::Image& img, int i);
        Node* LoadNodes(tinygltf::Model& m);
        void InitUniforms(tinygltf::Model& model);
        
};
