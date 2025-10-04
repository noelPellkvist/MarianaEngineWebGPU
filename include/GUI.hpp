#pragma once

#include <Window.hpp>
#include <Texture.hpp>
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