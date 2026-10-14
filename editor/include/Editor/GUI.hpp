#pragma once

#include <Mariana/Platform/Window.hpp>
#include <Mariana/Graphics/Texture.hpp>
#include <imgui.h>
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
        // For ImGui::Image: the texture's display view (as authored, no sRGB decode), one mip or all.
        static ImTextureID TextureId(Texture& texture, int mipLevel = -1);

};