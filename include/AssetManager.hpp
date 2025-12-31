#pragma once
#include <vector>
#include <memory>
#include <unordered_map>

#include <Texture.hpp>
#include <Material.hpp>
#include <Mesh.hpp>
#include <Logger.hpp>

class AssetManager
{
    public:
        static inline std::vector<Texture> LoadedTextures{};
        static inline std::vector<Material2> LoadedMaterials{};
        static inline std::vector<std::shared_ptr<IMesh>> LoadedMeshes{};

        AssetManager();
        ~AssetManager();

        size_t GetTextureCount() { return LoadedTextures.size(); }
        size_t GetMaterialCount() { return LoadedMaterials.size(); }
        size_t GetMeshCount() { return LoadedMeshes.size(); }
};