#pragma once
#include <vector>
#include <memory>
#include <unordered_map>

#include <Texture.hpp>
#include <Material.hpp>
#include <Mesh.hpp>
#include <Logger.hpp>

struct GLTFMaterialProperties
{
    glm::vec4 baseColor{1,1,1,1};
    float metallicFactor{0};
    float roughnessFactor{1};

    float normalMapStrength{1};
    float occlusionStrength{1};

    glm::vec3 emissiveFactor{0,0,0};
    float alphaCutoff{0.5};
    
};

class AssetManager
{
    public:
        static inline std::vector<Texture> LoadedTextures{};
        static inline std::vector<GLTFMaterialProperties> LoadedMaterialProperties{};
        static inline std::vector<Material2> LoadedMaterials{};
        static inline std::vector<std::shared_ptr<IMesh>> LoadedMeshes{};

        AssetManager();
        ~AssetManager();

        size_t GetTextureCount() { return LoadedTextures.size(); }
        size_t GetMaterialCount() { return LoadedMaterials.size(); }
        size_t GetMeshCount() { return LoadedMeshes.size(); }
};