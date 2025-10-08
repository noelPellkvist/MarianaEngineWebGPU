#pragma once
#include <vector>
#include <memory>
#include <unordered_map>

#include <Texture.hpp>
#include <Material.hpp>
#include <Mesh.hpp>

#include <Editor/GLTFLoader.hpp>

class AssetManager
{
    public:
        static inline std::vector<Texture> LoadedTextures{};
        static inline std::vector<std::shared_ptr<IMaterial>> LoadedMaterials{};
        static inline std::vector<std::shared_ptr<IMesh>> LoadedMeshes{};
        static inline std::vector<std::shared_ptr<IShader>> LoadedShaders{};

        static inline std::unordered_map<std::string, uint32_t> MaterialMap;
        static inline std::unordered_map<std::string, uint32_t> MeshMap;
        static inline std::unordered_map<std::string, uint32_t> ShaderMap;

        AssetManager();
        ~AssetManager();

        void AddShader(std::shared_ptr<IShader> newShader, std::string& name) { 
            auto [iter, success] = ShaderMap.insert({name, LoadedShaders.size()});
            if (!success) {
                Logger::Error("Shader already exists with that name");
            }
            LoadedShaders.push_back(newShader);
        }

        void AddMaterial(std::shared_ptr<IMaterial> newMaterial, std::string& name)
        { 
            auto [iter, success] = MaterialMap.insert({name, LoadedMaterials.size()});
            if (!success) {
                Logger::Error("Material already exists with that name");
            }
            LoadedMaterials.push_back(newMaterial);
        }

        void AddMesh(std::shared_ptr<IMesh> newMesh, std::string& name)
        { 
            auto [iter, success] = MeshMap.insert({name, LoadedMeshes.size()});
            if (!success) {
                Logger::Error("Mesh already exists with that name");
            }
            LoadedMeshes.push_back(newMesh);
        }

        uint32_t GetMaterialIndex(std::string name) { return MaterialMap[name]; }
        uint32_t GetMeshIndex(std::string name) { return MeshMap[name]; }
        uint32_t GetShaderIndex(std::string name) { return ShaderMap[name]; }

        size_t GetTextureCount() { return LoadedTextures.size(); }
        size_t GetMaterialCount() { return LoadedMaterials.size(); }
        size_t GetMeshCount() { return LoadedMeshes.size(); }
};