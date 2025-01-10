#pragma once

#include <vector>
#include <string>
#include <webgpu/webgpu_cpp.h>
#include <glm.hpp>
#include "GlobalVaribles.hpp"
#include "GameObject.hpp"
#include <chrono>


#include "../External/tiny_gltf.h"

class Model {
    public:
        Model(std::string name, bool bin=true);
        ~Model();

        void Draw(wgpu::RenderPassEncoder& renderPass);

        std::vector<Node*> rootNodes;

    private:
        std::vector<Node*> nodes;
        std::vector<MaterialProperties> materials;
        std::vector<MeshData> meshes;
        std::vector<Submesh> subMeshes;
        std::vector<ModelData> modelData;
        std::vector<wgpu::BindGroup> TextureBindings;
        std::vector<wgpu::TextureView> TextureViews;
        std::vector<AnimationData> animations;
        std::vector<glm::mat4> boneMatrices;
        std::vector<glm::mat4> inverseBindMatrices;
        std::vector<int> joints;
        uint32_t uniformStride;
        wgpu::Buffer modelsBuffer;
        wgpu::Buffer boneBuffer;
        wgpu::BindGroup modelDataBindGroup;
        wgpu::BindGroup boneBindGroup;

        void LoadNodes(tinygltf::Model& m);
        void TraverseNodes(Node* node, glm::mat4x4 parentMatrix = glm::mat4x4(1.0f));
        void UpdateNodes();
        void InitModelUniforms();
        void InitBonesBuffer(tinygltf::Model& m);
        void LoadMaterials(tinygltf::Model& m);
        void LoadMeshes(tinygltf::Model& m);
        void InitModelBindgroups();
        void InitBonesBindgroups();

        void LoadAnimations(tinygltf::Model& model);
        void UpdateAnimatedNodes();
        std::chrono::steady_clock::time_point startTime;

        void InitTextureBindgroups(); //TODO
};
