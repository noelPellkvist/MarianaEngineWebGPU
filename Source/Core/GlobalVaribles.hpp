#pragma once
#include <webgpu/webgpu_cpp.h>
#include <glm.hpp>
#include <gtc/quaternion.hpp>
#include <string>
#include <vector>
#include <utility>

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



struct Submesh
{
    int materialIndex;
    uint32_t startVertex;
    uint32_t vertexCount;
    uint32_t startIndex;
    uint32_t indexxCount;
};

struct MeshData
{
    std::vector<Submesh> submeshes;
    wgpu::Buffer vertexBuffer;
    wgpu::Buffer indexBuffer;
    uint32_t indexCount;
};

struct Node
{
    std::string name;
    std::vector<Node*> children;
    MeshData* mesh;
    Node* parent = nullptr;
    glm::vec3 localPosition = {};
    glm::quat localRotation = {};
    glm::vec3 localScale = {};
    glm::mat4x4 modelMatrix = {};    
};

enum AnimationChannelType
{
    TRANSLATION,
    ROTATION,
    SCALE,
    WEIGHTS
};

enum AnimationInterpolationType
{
    STEP,
    LINEAR,
    SPLINE
};

struct AnimationKeyFrames
{
    float time;
    std::vector<float> data;
    std::vector<float> inTangent;
    std::vector<float> outTangent;
};

struct AnimationChannel
{
    AnimationChannelType type;
    AnimationInterpolationType interpolationMode;
    int targetNodeIndex;
    std::vector<AnimationKeyFrames> keyFrames;

    std::pair<AnimationKeyFrames, AnimationKeyFrames> FindLeftRight(float time)
    {
        AnimationKeyFrames left = keyFrames[0];
        AnimationKeyFrames right = keyFrames.back();

        if(left.time >= time)
        {
            return std::make_pair(left, left);
        }
        if(right.time <= time)
        {
            return std::make_pair(right, right);
        }

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

        return std::make_pair(left, right);
    }

    glm::vec3 InterpolatePosition(float time)
    {
        std::pair<AnimationKeyFrames, AnimationKeyFrames> leftRight = FindLeftRight(time);
        AnimationKeyFrames& left = leftRight.first;
        AnimationKeyFrames& right = leftRight.second;

        glm::vec3 leftVec = { left.data[0], left.data[1], left.data[2]};
        glm::vec3 rightVec = { right.data[0], right.data[1], right.data[2]};

        if (interpolationMode == AnimationInterpolationType::LINEAR)
            return glm::mix(leftVec, rightVec, (time - left.time) / (right.time - left.time));

        if (interpolationMode == AnimationInterpolationType::SPLINE) {
            float t = (time - left.time) / (right.time - left.time);

            glm::vec3 leftTangent = { left.outTangent[0], left.outTangent[1], left.outTangent[2] };
            glm::vec3 rightTangent = { right.inTangent[0], right.inTangent[1], right.inTangent[2] };

            float t2 = t * t;
            float t3 = t2 * t;

            float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
            float h10 = t3 - 2.0f * t2 + t;          
            float h01 = -2.0f * t3 + 3.0f * t2;      
            float h11 = t3 - t2;                    

            return h00 * leftVec +
                   h10 * (right.time - left.time) * leftTangent +
                   h01 * rightVec +
                   h11 * (right.time - left.time) * rightTangent;
        }
        return leftVec;
    }

    glm::quat InterpolateRotation(float time)
{
    std::pair<AnimationKeyFrames, AnimationKeyFrames> leftRight = FindLeftRight(time);
    AnimationKeyFrames& left = leftRight.first;
    AnimationKeyFrames& right = leftRight.second;

    glm::quat leftQuat = glm::quat(left.data[3], left.data[0], left.data[1], left.data[2]);
    glm::quat rightQuat = glm::quat(right.data[3], right.data[0], right.data[1], right.data[2]);

    float t = (time - left.time) / (right.time - left.time);

    if (interpolationMode == AnimationInterpolationType::LINEAR)
        return glm::slerp(leftQuat, rightQuat, t);

    if (interpolationMode == AnimationInterpolationType::SPLINE) {
        glm::quat leftTangent = glm::quat(left.outTangent[3], left.outTangent[0], left.outTangent[1], left.outTangent[2]);
        glm::quat rightTangent = glm::quat(right.inTangent[3], right.inTangent[0], right.inTangent[1], right.inTangent[2]);

        if (glm::dot(leftQuat, rightQuat) < 0.0f) {
            rightQuat = -rightQuat; 
        }

        float t2 = t * t;
        float t3 = t2 * t;

        float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f; 
        float h10 = t3 - 2.0f * t2 + t;          
        float h01 = -2.0f * t3 + 3.0f * t2;     
        float h11 = t3 - t2;                    

        glm::quat interpolatedQuat =
            glm::normalize(h00 * leftQuat +
                           h10 * (right.time - left.time) * leftTangent +
                           h01 * rightQuat +
                           h11 * (right.time - left.time) * rightTangent);

        return interpolatedQuat;
    }

    return leftQuat;
}

};

struct AnimationData
{
    std::vector<AnimationChannel> channels;
    std::string name;
};
