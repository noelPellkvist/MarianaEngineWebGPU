#pragma once
#include <Renderpass.hpp>
#include <ICamera.hpp>
#include <Mesh.hpp>

class Scene;
class IShader;
class GUI;

class Renderer
{
    public:
        Renderer();
        ~Renderer();

        void Render(ICamera& camera, Renderpass& renderPass, GUI gui, IShader& shader);
};