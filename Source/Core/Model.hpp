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
        std::vector<Node*> DrawableNodes;
        std::vector<MaterialProperties> materials;
        std::vector<MeshData> meshes;
        std::vector<ModelData> modelData;
        std::vector<AnimationData> animations;
        
        uint32_t uniformStride;
        wgpu::Buffer modelsBuffer;
        wgpu::BindGroup modelDataBindGroup;
        float animationLength = 0;


        std::vector<int> joints;
        std::vector<glm::mat4> inverseBindMatrices;
        std::vector<glm::mat4> jointMatrices;
        wgpu::Buffer boneBuffer;
        wgpu::BindGroup boneBindGroup;

        void LoadNodes(tinygltf::Model& m);
        void TraverseNodes(Node* node, glm::mat4x4 parentMatrix = glm::mat4x4(1.0f));
        void UpdateNodes();
        void InitUniforms();
        void LoadMaterials(tinygltf::Model& m);
        void LoadMeshes(tinygltf::Model& m);
        void InitModelBindgroups();

        void LoadSkin(tinygltf::Model& m);
        void FixJointMatrices();

        void LoadAnimations(tinygltf::Model& model);
        void UpdateAnimatedNodes();
        std::chrono::steady_clock::time_point startTime;
};
