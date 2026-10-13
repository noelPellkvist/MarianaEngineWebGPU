#pragma once

#include <Mariana/Platform/Window.hpp>
#include <Mariana/Graphics/Texture.hpp>
namespace wgpu { class RenderPassEncoder; }
class GUI
{
    public:
        GUI();
        ~GUI();

        void InitGui(Window& window);
        void PreUpdateGUI();
        void PostUpdateGUI(wgpu::RenderPassEncoder* renderPass);
        void KillGui();

        void DrawTexture(Texture texture, float width, float height);

};