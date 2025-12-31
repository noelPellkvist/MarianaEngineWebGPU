#pragma once
#include <string>
#include <glm/glm.hpp>

#include <Mesh.hpp>
#include <Texture.hpp>
#include <Shader.hpp>

namespace GLTF
{

struct Vertex {
    glm::vec3 position;   // POSITION
    glm::vec3 normal;     // NORMAL
    glm::vec4 tangent;    // TANGENT (xyz + handedness in w)
    glm::vec2 texcoord0;  // TEXCOORD_0
    glm::vec2 texcoord1;  // TEXCOORD_1 (optional)
    glm::vec4 color0;     // COLOR_0 (normalized RGBA)

    // Equality operator (handy for deduplication when building index buffers)
    bool operator==(const Vertex& other) const {
        return position == other.position &&
               normal == other.normal &&
               tangent == other.tangent &&
               texcoord0 == other.texcoord0 &&
               texcoord1 == other.texcoord1 &&
               color0 == other.color0;
    }
};

struct GLTFMaterialProperties
{
    glm::vec4 baseColor{1,1,1,1};
    float metallicFactor{1};
    float roughnessFactor{1};

    float normalMapStrength{1};
    float occlusionStrength{1};

    glm::vec3 emissiveFactor{0,0,0};
    float alphaCutoff{0.5};
    
};

class GLTFLoader
{
    public:
        static void LoadGLTF(std::string filename, Shader2& shader);
};

} // namespace GLTF