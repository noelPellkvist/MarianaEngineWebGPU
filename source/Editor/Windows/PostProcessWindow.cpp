#include "Editor/Windows/PostProcessWindow.hpp"

#include <imgui.h>

PostProcessWindow::PostProcessWindow(GUI& gui, PostProcessPipeline& postProcess, AmbientOcclusion& ambientOcclusion)
    : EditorWindow("Post Processing", gui, false),
      m_postProcess(postProcess),
      m_ambientOcclusion(ambientOcclusion)
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

        ImGui::PushID("AO");
        ImGui::SeparatorText("Ambient Occlusion (GTAO)");
        ImGui::Checkbox("Enabled", &m_ambientOcclusion.enabled);
        ImGui::BeginDisabled(!m_ambientOcclusion.enabled);
        ImGui::DragFloat("Radius", &m_ambientOcclusion.radius, 0.01f, 0.05f, 5.0f, "%.2f m");
        ImGui::SliderFloat("Power", &m_ambientOcclusion.power, 0.5f, 4.0f, "%.2f");
        ImGui::EndDisabled();
        ImGui::PopID();

        ImGui::SeparatorText("Anti-aliasing");
        ImGui::TextDisabled("4x MSAA, alpha to coverage on alpha-tested materials");
    }
    ImGui::End();
}
