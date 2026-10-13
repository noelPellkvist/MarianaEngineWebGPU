#pragma once
#include <Renderpass.hpp>
#include <ECS.hpp>
#include <AssetsSystem/AssetManager.hpp>
#include <memory>
#include <vector>

class Scene;
class IShader;
class GUI;
class IMesh;
class MaterialAsset;

struct MeshComponent {
    AssetHandle<IMesh> mesh;
    // Material for each submesh, indexed by Submesh::materialIndex. Missing or empty entries
    // render with the default material.
    std::vector<AssetHandle<MaterialAsset>> materials;
};

struct ShadowCasterTag {};

class Renderer
{
    public:
        Renderer();
        ~Renderer();

        void PushRenderpass(Renderpass* renderpass) { m_Renderpasses.push_back(renderpass); }

        void Render(GUI* gui);

    private:
        std::vector<Renderpass*> m_Renderpasses;
        
};