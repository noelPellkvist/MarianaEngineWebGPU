#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <glm/glm.hpp>

#include <Mesh.hpp>
#include <Texture.hpp>
#include <Shader.hpp>
#include <Prefab.hpp>
#include <AssetsSystem/AssetManager.hpp>

namespace GLTF
{

struct Vertex {
    glm::vec3 position;   // POSITION
    glm::vec3 normal;     // NORMAL
    glm::vec4 tangent;    // TANGENT (xyz + handedness in w)
    glm::vec2 texcoord0;  // TEXCOORD_0
    glm::vec2 texcoord1;  // TEXCOORD_1 (optional)
    glm::vec4 color0;     // COLOR_0 (normalized RGBA)
    glm::uvec4 boneIndices;  // JOINTS_0 (optional, up to 4 bone indices)
    glm::vec4 boneWeights;   // WEIGHTS_0 (optional, up to 4 bone weights)

    // Equality operator (handy for deduplication when building index buffers)
    bool operator==(const Vertex& other) const {
        return position == other.position &&
               normal == other.normal &&
               tangent == other.tangent &&
               texcoord0 == other.texcoord0 &&
               texcoord1 == other.texcoord1 &&
               color0 == other.color0 &&
               boneIndices == other.boneIndices &&
               boneWeights == other.boneWeights;
    }
};



class GLTFLoader
{
    public:
        // Parses a .gltf/.glb file into a prefab. Its textures, materials, meshes and animations
        // are registered with `assets` under "<file>#<kind>/<index>" keys, so loading the file
        // again reuses whichever of them are still loaded. Returns nullptr on failure.
        static std::shared_ptr<Prefab> LoadGLTF(AssetManager2& assets, const std::filesystem::path& file, Shader2& shader);

        // Makes AssetManager2::Load<Prefab>() handle .gltf and .glb files.
        static void RegisterLoader(AssetManager2& assets, Shader2& shader);
};

} // namespace GLTF
