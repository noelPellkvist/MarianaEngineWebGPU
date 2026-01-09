#pragma once
#include <Renderpass.hpp>
#include <ECS.hpp>
#include <memory>
#include <vector>

class Scene;
class IShader;
class GUI;

struct MeshComponent {
    uint32_t meshIndex{0};
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