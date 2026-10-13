#include "Editor/Windows/PostProcessWindow.hpp"

#include <imgui.h>

PostProcessWindow::PostProcessWindow(GUI& gui, PostProcessPipeline& postProcess)
    : EditorWindow("Post Processing", gui, false),
      m_postProcess(postProcess)
{
}

void PostProcessWindow::Draw()
{
    if (!m_open)
        return;

    if (ImGui::Begin(m_name.c_str(), &m_open))
    {
        ImGui::DragFloat("Exposure", &m_postProcess.exposure, 0.01f, 0.0f, 20.0f, "%.2f");

        ImGui::SeparatorText("Bloom");
        ImGui::Checkbox("Enabled", &m_postProcess.bloomEnabled);
        ImGui::BeginDisabled(!m_postProcess.bloomEnabled);
        ImGui::SliderFloat("Intensity", &m_postProcess.bloomIntensity, 0.0f, 4.0f, "%.2f");
        ImGui::DragFloat("Threshold", &m_postProcess.bloomThreshold, 0.01f, 0.0f, 20.0f, "%.2f");
        ImGui::SliderFloat("Knee", &m_postProcess.bloomKnee, 0.0f, 2.0f, "%.2f");
        ImGui::SliderFloat("Radius", &m_postProcess.bloomRadius, 0.25f, 4.0f, "%.2f");
        ImGui::EndDisabled();
    }
    ImGui::End();
}
