#pragma once
#include <Renderpass.hpp>

class Scene;
class IShader;
class GUI;

class Renderer
{
    public:
        Renderer();
        ~Renderer();

        void Render(Renderpass& renderPass, GUI gui, IShader& shader);
};