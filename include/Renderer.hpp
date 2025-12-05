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

        void PushRenderpass(Renderpass* renderpass) { m_Renderpasses.push_back(renderpass); }

        void Render(GUI* gui);

    private:
        std::vector<Renderpass*> m_Renderpasses;
        
};