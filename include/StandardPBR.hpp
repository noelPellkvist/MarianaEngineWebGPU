#pragma once
#include <glm/glm.hpp>
#include <AssetManager.hpp>
#include <ICamera.hpp>
#include <Editor/GLTFLoader.hpp>

struct UBO {
  glm::vec3 lightDir;
  glm::mat4 lightVP;
};

struct TransformData {
  glm::mat4x4 modelMatrix;
  glm::mat3x3 normalMatrix;
  uint32_t entityID{0};
};

struct BoneData
{
    glm::mat4x4 model = glm::mat4x4(1.0f);
    glm::mat4x4 normal = glm::mat4x4(1.0f);
};

struct DrawData
{
    uint32_t transformIndex{0};
    uint32_t materialIndex{0};
};

class StandardPBRPipeline
{
        private:
        void BuildBuffers();

        UBO ubo{};
        TransformData transformData{};
        GLTFMaterialProperties materialData{};
        CameraInfo cameraInfo{};
        BoneData boneData{};
        DrawData drawData{};
        GLTF::Vertex v{};

        UniformBufferLayout uboLayout;
        StorageArrayLayout transformLayout;
        StorageArrayLayout materialsLayout;
        UniformBufferLayout camLayout;
        StorageArrayLayout boneLayout;
        StorageArrayLayout drawLayout;
        VertexBufferLayout vertexLayout;

    public:
        StandardPBRPipeline();
        ~StandardPBRPipeline();

        Buffer uboBuffer;
        Buffer transformBuffer;
        Buffer materialsBuffer;
        Buffer cameraBuffer;
        Buffer boneBuffer;
        Buffer drawBuffer;


    };