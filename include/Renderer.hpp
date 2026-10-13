#pragma once
#include <Renderpass.hpp>
#include <Rendering/RenderGraph.hpp>
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

// Draws a frame: executes the render graph (whatever writes "Backbuffer" ends up on screen), then
// the UI on top of it.
class Renderer
{
    public:
        Renderer();
        ~Renderer();

        // Set its backbuffer format before adding passes that render to the screen.
        RenderGraph graph;

        void Render(GUI* gui);
};