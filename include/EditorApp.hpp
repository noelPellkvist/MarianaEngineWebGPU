#pragma once
#include <Application.hpp>
#include <Shader.hpp>
#include <Material.hpp>
#include <Renderer.hpp>
#include <moved_later/GLTFLoader.hpp>
#include <Mesh.hpp>
#include <vector>
#include <UniformLayout.hpp>

class EditorApp : public Application
{
    public:
        EditorApp(const std::string& name);
        ~EditorApp() override;

    protected:
        void OnStart() override;
        void OnUpdate(float deltaTime) override;
        void OnRender() override;
        void OnGUI() override;
        void OnShutdown() override;

    private:
        std::vector<Texture> LoadedTextures;
        std::vector<IUniformLayout> uniformBuffers;
        Shader PBR_Shader;
        Material material;
        Renderer renderer;
        Mesh<GLTF::Vertex, uint32_t> mesh;
        Renderpass renderpass;
};