#pragma once
#include <webgpu/webgpu_cpp.h>
#include <glm.hpp>
#include <gtc/quaternion.hpp>
#include <string>
#include <vector>

#define GLM_ENABLE_EXPERIMENTAL

extern wgpu::Instance instance;
extern wgpu::Adapter adapter;
extern wgpu::Device device;

uint32_t ceilToNextMultiple(uint32_t value, uint32_t step);

 struct UBO 
 {
    glm::mat4x4 projectionMatrix;
    glm::mat4x4 viewMatrix;
    glm::mat4x4 modelMatrix;
    float color[4];
    float time;
    float _pad[3];
};

struct Vertex 
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color = {1,1,1};
    glm::vec2 uv;
};

struct MaterialProperties
{
    glm::vec4 baseColorFactor;
    alignas(16) glm::vec3 emissiveFactor;
    float alphaCutoff;
    float metallicFactor;
    float roughnessFactor;
};

struct ModelData
{
    glm::mat4x4 modelMatrix;
    MaterialProperties materialProps;
};

struct Node
{
    std::string name;
    std::vector<Node*> children;
    Node* parent = nullptr;
    glm::vec3 localPosition = {};
    glm::quat localRotation = {};
    glm::vec3 localScale = {};
    glm::mat4x4 modelMatrix = {};    
};

struct NodesMesh
{
    wgpu::Buffer vertexBuffer;
    wgpu::Buffer indexBuffer;
    uint32_t indexCount;
    int matIndex = -1;
    int nodeIndex = -1;
};