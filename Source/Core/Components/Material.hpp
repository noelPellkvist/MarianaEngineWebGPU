#pragma once
#include <glm.hpp>
#include <webgpu/webgpu_cpp.h>

struct MaterialBufferData
{
    glm::vec4 baseColorFactor; 
    float metallicFactor; 
    float roughnessFactor; 
    float _pad0[2];
    glm::vec3 emissiveFactor;
    float _pad1;
};

struct MaterialComponent
{
    int baseColorTextureIndex{-1};
    int metallicRoughnessTexture{-1};
    int normalTexture{-1};
    int occlusionTexture{-1};
    int emissiveTexture{-1};
    int dataIndex;
    MaterialBufferData uniform;
};