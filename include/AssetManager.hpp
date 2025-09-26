#pragma once
#include <vector>
#include <memory>

#include <Texture.hpp>
#include <Material.hpp>
#include <Mesh.hpp>

#include <moved_later/GLTFLoader.hpp>

class AssetManager
{
    public:
        static inline std::vector<Texture> LoadedTextures{};
        static inline std::vector<MaterialInstance> LoadedMaterials{};
        static inline std::vector<Mesh<GLTF::Vertex, uint32_t>> LoadedMeshes{};

        AssetManager();
        ~AssetManager();

        size_t GetTextureCount() { return LoadedTextures.size(); }
        size_t GetMaterialCount() { return LoadedMaterials.size(); }
        size_t GetMeshCount() { return LoadedMeshes.size(); }
};