#include "Editor/Windows/PostProcessWindow.hpp"

#include <ICamera.hpp>
#include <imgui.h>

#include <filesystem>
#include <string>

PostProcessWindow::PostProcessWindow(GUI& gui, EffectStack& effects, ICamera& camera)
    : EditorWindow("Post Processing", gui, false),
      m_effects(effects),
      m_camera(camera)
{
}

void PostProcessWindow::DrawEffect(EffectInstance& effect, bool& removed)
{
    ImGui::PushID(&effect);
    const bool open = ImGui::CollapsingHeader(effect.Name().c_str(), ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_DefaultOpen);

    // Toggle, reorder and remove sit on the header row.
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 90.0f);
    ImGui::Checkbox("##enabled", &effect.enabled);
    ImGui::SameLine();
    if (ImGui::ArrowButton("##up", ImGuiDir_Up))
        m_effects.Move(&effect, -1);
    ImGui::SameLine();
    if (ImGui::ArrowButton("##down", ImGuiDir_Down))
        m_effects.Move(&effect, 1);
    ImGui::SameLine();
    if (ImGui::SmallButton("x"))
        removed = true;

    if (open)
    {
        ImGui::BeginDisabled(!effect.enabled);
        for (size_t i = 0; i < effect.ParamCount(); ++i)
        {
            const EffectParamInfo& info = effect.ParamInfo(i);
            glm::vec4& value = effect.ParamValue(i);
            const char* label = info.name.c_str();
            const float speed = (info.max - info.min) * 0.002f;
            switch (info.type)
            {
                case EffectParamType::Float: ImGui::SliderFloat(label, &value.x, info.min, info.max, "%.3f"); break;
                case EffectParamType::Int:
                {
                    int x = static_cast<int>(value.x);
                    if (ImGui::SliderInt(label, &x, static_cast<int>(info.min), static_cast<int>(info.max)))
                        value.x = static_cast<float>(x);
                    break;
                }
                case EffectParamType::Bool:
                {
                    bool on = value.x != 0.0f;
                    if (ImGui::Checkbox(label, &on))
                        value.x = on ? 1.0f : 0.0f;
                    break;
                }
                case EffectParamType::Vec2: ImGui::DragFloat2(label, &value.x, speed, info.min, info.max); break;
                case EffectParamType::Vec3: ImGui::DragFloat3(label, &value.x, speed, info.min, info.max); break;
                case EffectParamType::Vec4: ImGui::DragFloat4(label, &value.x, speed, info.min, info.max); break;
                case EffectParamType::Color3: ImGui::ColorEdit3(label, &value.x, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float); break;
                case EffectParamType::Color4: ImGui::ColorEdit4(label, &value.x, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float); break;
            }
        }
        if (effect.ParamCount() == 0)
            ImGui::TextDisabled("No parameters");
        ImGui::TextDisabled("%s", effect.Definition().path.c_str());
        ImGui::EndDisabled();
    }
    ImGui::PopID();
}

void PostProcessWindow::Draw()
{
    if (!m_open)
        return;

    if (ImGui::Begin(m_name.c_str(), &m_open))
    {
        float exposure = m_camera.Exposure();
        if (ImGui::DragFloat("Exposure", &exposure, 0.01f, 0.0f, 20.0f, "%.2f"))
            m_camera.SetExposure(exposure);
        ImGui::TextDisabled("Anti-aliasing: 4x MSAA, alpha to coverage on alpha-tested materials");

        EffectInstance* toRemove = nullptr;
        for (EffectStage stage : { EffectStage::BeforeLighting, EffectStage::PostProcess })
        {
            ImGui::SeparatorText(stage == EffectStage::BeforeLighting ? "Before lighting" : "Post-process");
            for (const auto& effect : m_effects.Effects())
            {
                if (effect->Definition().stage != stage)
                    continue;
                bool removed = false;
                DrawEffect(*effect, removed);
                if (removed)
                    toRemove = effect.get();
            }
        }
        if (toRemove)
            m_effects.Remove(toRemove);

        ImGui::Separator();
        if (ImGui::BeginCombo("##add", "Add effect..."))
        {
            std::error_code error;
            for (const auto& entry : std::filesystem::directory_iterator(std::string(RESOURCE_DIR) + "/Effects", error))
            {
                if (entry.path().extension() != ".effect")
                    continue;
                const std::string file = entry.path().filename().string();
                if (ImGui::Selectable(file.c_str()))
                    m_effects.Add("/Effects/" + file);
            }
            ImGui::EndCombo();
        }
    }
    ImGui::End();
}
