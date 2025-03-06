#pragma once
#include <glm.hpp>
#include <string>

struct MaterialData
{
    glm::vec4 baseColorFactor = {1,1,1,1};
};

struct Material
{
    std::string name;
    //PBR metallic roughness
    glm::vec4 baseColorFactor = {1,1,1,1};
    float metallicFactor = 1;
    float roughnessFactor = 1;
    //end of PBR metallic roughness
    glm::vec3 emissiveFactor = {0,0,0};
    std::string alphaMode = "OPAQUE";
    float alphaCutoff = 0.5f;
    bool doubleSided = false;
};
