#pragma once
#include <Renderpass.hpp>

class Scene;
class IShader;
class GUI;
class Entity;

struct RendererComponent
{
    uint32_t shaderIndex{0};
    uint32_t meshIndex{0};
};

class Renderer
{
    public:
        Renderer();
        ~Renderer();

        void Render(Renderpass& renderPass, GUI gui, RendererComponent& rendererComp);
};