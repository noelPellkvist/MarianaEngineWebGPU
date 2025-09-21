#pragma once
#include <string>
#include <glm/glm.hpp>

#include <Mesh.hpp>
#include <Texture.hpp>

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

class GLTFLoader
{
    public:
        static Mesh<Vertex, uint32_t> LoadFromFile(std::string filename);
        static std::vector<Texture> LoadTexturesFromFile(std::string filename);
};

} // namespace GLTF