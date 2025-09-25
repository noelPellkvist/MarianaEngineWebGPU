#pragma once
#include <Application.hpp>
#include <Shader.hpp>
#include <Material.hpp>
#include <Renderer.hpp>
#include <moved_later/GLTFLoader.hpp>
#include <Mesh.hpp>
#include <vector>
#include <UniformLayout.hpp>
#include <ECS.hpp>
#include <memory>

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
        std::vector<IUniformLayout> uniformBuffers;
        Shader PBR_Shader;
        Material material;
        Renderer renderer;
        Scene scene;
        uint64_t selectedEntityID = -1;
        
        Mesh<GLTF::Vertex, uint32_t> mesh;
        Renderpass renderpass;

        void DrawEntityNode(Entity& e);
};