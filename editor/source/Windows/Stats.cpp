#include <Editor/Windows/Stats.hpp>

#include <imgui.h>

StatsWindow::StatsWindow(GUI& gui, std::function<void()> openProfiler)
    : EditorWindow("Stats", gui, true), m_OpenProfiler(std::move(openProfiler))
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
        if (m_OpenProfiler && ImGui::Button("Profiler..."))
            m_OpenProfiler();
    }
    ImGui::End();
}
