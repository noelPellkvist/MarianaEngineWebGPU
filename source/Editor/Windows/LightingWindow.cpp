#include "Editor/Windows/LightingWindow.hpp"

#include <imgui.h>

LightingWindow::LightingWindow(GUI& gui, LightingSettings& lighting)
    : EditorWindow("Lighting", gui, false),
      m_lighting(lighting)
{
}

void LightingWindow::Draw()
{
    if (!m_open)
        return;

    if (ImGui::Begin(m_name.c_str(), &m_open))
    {
        ImGui::SeparatorText("Sun");
        ImGui::SliderFloat("Azimuth", &m_lighting.sunAzimuth, 0.0f, 360.0f, "%.0f deg");
        ImGui::SliderFloat("Elevation", &m_lighting.sunElevation, 1.0f, 90.0f, "%.0f deg");
        ImGui::ColorEdit3("Color", &m_lighting.sunColor.x);
        ImGui::DragFloat("Intensity", &m_lighting.sunIntensity, 0.01f, 0.0f, 20.0f, "%.2f");

        ImGui::SeparatorText("Sky");
        ImGui::DragFloat("Diffuse", &m_lighting.skyIntensity, 0.01f, 0.0f, 10.0f, "%.2f");
        ImGui::DragFloat("Reflections", &m_lighting.reflectionIntensity, 0.01f, 0.0f, 10.0f, "%.2f");
    }
    ImGui::End();
}
