#pragma once
#include <Application.hpp>
#include <Shader.hpp>
#include <Material.hpp>
#include <Renderer.hpp>
#include <Editor/GLTFLoader.hpp>
#include <Mesh.hpp>
#include <vector>
#include <UniformLayout.hpp>
#include <ECS.hpp>
#include <memory>
#include <Texture.hpp>

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
        std::shared_ptr<IShader> PBR_Shader;
        std::vector<Texture> AssetsTextures;
        Renderer renderer;
        Scene scene;
        uint64_t selectedEntityID = -1;
        Entity selectedEntity;
        Renderpass renderpass;
        
        void OnWindowResized(int w, int h);

        void LoadFileTextures();

        void DrawEntityNode(Entity& e);
        void DrawInspector(Entity& e);
        void DrawTopMenu();
        void DrawAssetsWindow();
        void DrawMat4(const char* id, float m[16], bool editable = true, float speed = 0.05f, const char* fmt = "%.3f");
};