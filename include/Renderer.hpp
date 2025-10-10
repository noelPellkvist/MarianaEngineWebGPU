#pragma once
#include <Renderpass.hpp>
#include <ECS.hpp>
#include <memory>

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

        void Render(Renderpass& renderPass, GUI& gui);

    private:
        System renderSystem;
        struct Impl;
        std::unique_ptr<Impl> _impl;
        
};