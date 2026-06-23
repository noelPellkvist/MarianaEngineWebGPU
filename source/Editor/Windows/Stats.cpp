#include "Editor/Windows/Stats.hpp"

#include <imgui.h>

StatsWindow::StatsWindow(GUI& gui)
    : EditorWindow("Stats", gui, true)
{
}

void StatsWindow::Draw()
{
    if (!m_open)
        return;

    if (ImGui::Begin(m_name.c_str(), &m_open))
    {
        float fps = ImGui::GetIO().Framerate;
        float ms = (fps > 0.0f) ? (1000.0f / fps) : 0.0f;
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", ms, fps);
    }
    ImGui::End();
}
