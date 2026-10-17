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

        // The dock space over the main window (set each frame by PreUpdateGUI), and its central
        // node, where the views dock the first time they open.
        ImGuiID DockSpaceId() const { return m_DockSpaceId; }
        ImGuiID CentralNodeId() const;

    private:
        ImGuiID m_DockSpaceId = 0;
};