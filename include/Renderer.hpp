#pragma once
#include <Renderpass.hpp>
#include <ECS.hpp>
#include <memory>
#include <vector>

class Scene;
class IShader;
class GUI;

struct RendererComponent
{
    uint32_t shaderIndex{0};
    uint32_t meshIndex{0};
    uint32_t transformIndex{0}; 
};

class Renderer
{
    public:
        Renderer();
        ~Renderer();

        void Init(Scene& scene);
        void PushRenderpass(Renderpass* renderpass) { m_Renderpasses.push_back(renderpass); }

        void Render(GUI* gui);

    private:
        System renderSystem;
        std::vector<Renderpass*> m_Renderpasses;
        struct Impl;
        std::unique_ptr<Impl> _impl;
        
};