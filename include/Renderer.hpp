#pragma once
#include <Renderpass.hpp>
#include <ICamera.hpp>
#include <Mesh.hpp>

class Scene;
class Material;
class Shader;
class GUI;

class Renderer
{
    public:
        Renderer();
        ~Renderer();

        void Render(ICamera& camera, Renderpass& renderPass, GUI gui, Material& mat, Shader& shader, IMesh& mesh);
};