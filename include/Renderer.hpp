#pragma once
#include <Renderpass.hpp>

class Scene;
class Material;
class Shader;
class GUI;

class Renderer
{
    public:
        Renderer();
        ~Renderer();

        void Render(Renderpass& renderPass, GUI gui, Material& mat, Shader& shader, wgpu::Buffer vertexBuffer, wgpu::Buffer indexBuffer, uint32_t IndexCount);
};