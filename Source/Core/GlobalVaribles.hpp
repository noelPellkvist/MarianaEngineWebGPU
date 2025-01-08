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
    glm::vec3 normal = {1,1,1};
    glm::vec3 color = {1,1,1};
    glm::vec2 uv = {1,1};
};

struct MaterialProperties
{
    glm::vec4 baseColorFactor = {1,1,1,1};
    alignas(16) glm::vec3 emissiveFactor = {1,1,1};
    float alphaCutoff = 0.5f;
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    uint32_t textureFlags = 0;
};

struct ModelData
{
    glm::mat4x4 modelMatrix = {};    
    MaterialProperties material = {};
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

struct Submesh
{
    int materialIndex;
    int nodeIndex;
    int meshIndex;
    uint32_t startVertex;
    uint32_t vertexCount;
    uint32_t startIndex;
    uint32_t indexxCount;
};

struct MeshData
{
    int nodeIndex;
    wgpu::Buffer vertexBuffer;
    wgpu::Buffer indexBuffer;
    uint32_t indexCount;
};

enum AnimationChannelType
{
    TRANSLATION,
    ROTATION,
    SCALE,
    WEIGHTS
};

struct AnimationKeyFrames
{
    float time;
    std::vector<float> data;
};

struct AnimationChannel
{
    AnimationChannelType type;
    int targetNodeIndex;
    std::vector<AnimationKeyFrames> keyFrames;

    glm::vec3 InterpolatePosition(float time)
    {
        AnimationKeyFrames left = keyFrames[0];
        AnimationKeyFrames right = keyFrames.back();

        glm::vec3 leftVec = { left.data[0], left.data[1], left.data[2]};
        glm::vec3 rightVec = { right.data[0], right.data[1], right.data[2]};

        if(left.time >= time) return leftVec;
        if(right.time <= time) return rightVec;

        for(int i = 0; i < keyFrames.size(); i++)
        {
            if (keyFrames[i].time > left.time && keyFrames[i].time <= time)
            {
                left = keyFrames[i];
            }
            if (keyFrames[i].time < right.time && keyFrames[i].time >= time)
            {
                right = keyFrames[i];
            }
        }

        leftVec = { left.data[0], left.data[1], left.data[2]};
        rightVec = { right.data[0], right.data[1], right.data[2]};

        return glm::mix(leftVec, rightVec, (time - left.time) / (right.time - left.time));
    }

    glm::quat InterpolateRotation(float time)
    {
        AnimationKeyFrames left = keyFrames[0];
        AnimationKeyFrames right = keyFrames.back();

        glm::quat leftQuat = glm::quat(left.data[3], left.data[0], left.data[1], left.data[2]);
        glm::quat rightQuat = glm::quat(right.data[3], right.data[0], right.data[1], right.data[2]);

        if (left.time >= time) return leftQuat;
        if (right.time <= time) return rightQuat;

        for (int i = 0; i < keyFrames.size(); i++)
        {
            if (keyFrames[i].time > left.time && keyFrames[i].time <= time)
            {
                left = keyFrames[i];
            }
            if (keyFrames[i].time < right.time && keyFrames[i].time >= time)
            {
                right = keyFrames[i];
            }
        }

        leftQuat = glm::quat(left.data[3], left.data[0], left.data[1], left.data[2]);
        rightQuat = glm::quat(right.data[3], right.data[0], right.data[1], right.data[2]);

        float t = (time - left.time) / (right.time - left.time);

        // Use glm::slerp for quaternion interpolation
        return glm::slerp(leftQuat, rightQuat, t);
    }
};

struct AnimationData
{
    std::vector<AnimationChannel> channels;
    std::string name;
};
