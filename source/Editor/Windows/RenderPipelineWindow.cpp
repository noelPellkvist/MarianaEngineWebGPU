#include "Editor/Windows/RenderPipelineWindow.hpp"

#include <ICamera.hpp>
#include <Logger.hpp>
#include <Rendering/RenderPipeline.hpp>
#include <imgui.h>

#include <cctype>
#include <filesystem>
#include <string>
#include <utility>

RenderPipelineWindow::RenderPipelineWindow(GUI& gui, EffectStack& effects, SceneRenderer& sceneRenderer, ICamera& camera, std::string pipelinePath)
    : EditorWindow("Render Pipeline", gui, false),
      m_effects(effects),
      m_sceneRenderer(sceneRenderer),
      m_camera(camera),
      m_pipelinePath(std::move(pipelinePath))
{
}

void RenderPipelineWindow::LoadPipeline(const std::string& path)
{
    RenderPipelineDesc desc;
    std::string error;
    if (!RenderPipelineDesc::Load(path, desc, error))
    {
        Logger::Error(error);
        m_status = error;
        return;
    }
    desc.Apply(m_effects, m_sceneRenderer, &m_camera);
    m_pipelinePath = path;
    m_status = "Loaded " + path;
}

void RenderPipelineWindow::SavePipeline(const std::string& path)
{
    RenderPipelineDesc desc = RenderPipelineDesc::Capture(m_effects, m_sceneRenderer, &m_camera);
    desc.name = std::filesystem::path(path).stem().string();
    std::string error;
    if (!desc.Save(path, error))
    {
        Logger::Error(error);
        m_status = error;
        return;
    }
    m_pipelinePath = path;
    m_status = "Saved " + path;
}

void RenderPipelineWindow::DrawPipelineFile()
{
    const std::string current = std::filesystem::path(m_pipelinePath).filename().string();
    if (ImGui::BeginCombo("Pipeline", current.empty() ? "None" : current.c_str()))
    {
        std::error_code error;
        for (const auto& entry : std::filesystem::directory_iterator(std::string(RESOURCE_DIR) + "/Pipelines", error))
        {
            if (entry.path().extension() != ".pipeline")
                continue;
            const std::string file = entry.path().filename().string();
            if (ImGui::Selectable(file.c_str(), file == current))
                LoadPipeline("/Pipelines/" + file);
        }
        ImGui::EndCombo();
    }

    ImGui::BeginDisabled(m_pipelinePath.empty());
    if (ImGui::Button("Save"))
        SavePipeline(m_pipelinePath);
    ImGui::SameLine();
    if (ImGui::Button("Reload"))
        LoadPipeline(m_pipelinePath);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f);
    ImGui::InputTextWithHint("##saveName", "new pipeline name", m_saveName, sizeof(m_saveName));
    ImGui::SameLine();
    ImGui::BeginDisabled(m_saveName[0] == '\0');
    if (ImGui::Button("Save As"))
    {
        // Only a file name, saved next to the others.
        std::string name = m_saveName;
        for (char& c : name)
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_' && c != ' ')
                c = '_';
        SavePipeline("/Pipelines/" + name + ".pipeline");
    }
    ImGui::EndDisabled();
    if (!m_status.empty())
        ImGui::TextDisabled("%s", m_status.c_str());
}

void RenderPipelineWindow::DrawSceneSettings()
{
    SceneRendererSettings settings = m_sceneRenderer.Settings();
    bool changed = ImGui::Checkbox("Shadows", &settings.shadows);
    ImGui::BeginDisabled(!settings.shadows);
    changed |= ImGui::SliderFloat("Shadow Distance", &settings.shadowDistance, 5.0f, 500.0f, "%.0f m", ImGuiSliderFlags_Logarithmic);
    const std::string size = std::to_string(settings.shadowMapSize);
    if (ImGui::BeginCombo("Shadow Map Size", size.c_str()))
    {
        for (uint32_t option : { 512u, 1024u, 2048u, 4096u })
        {
            if (ImGui::Selectable(std::to_string(option).c_str(), option == settings.shadowMapSize))
            {
                settings.shadowMapSize = option;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    if (changed)
        m_sceneRenderer.SetSettings(settings);
    ImGui::TextDisabled("Anti-aliasing: 4x MSAA, alpha to coverage on alpha-tested materials");
}

void RenderPipelineWindow::DrawEffect(EffectInstance& effect, bool& removed)
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

void RenderPipelineWindow::Draw()
{
    if (!m_open)
        return;

    if (ImGui::Begin(m_name.c_str(), &m_open))
    {
        DrawPipelineFile();

        ImGui::SeparatorText("Camera");
        float exposure = m_camera.Exposure();
        if (ImGui::DragFloat("Exposure", &exposure, 0.01f, 0.0f, 20.0f, "%.2f"))
            m_camera.SetExposure(exposure);

        ImGui::SeparatorText("Scene");
        DrawSceneSettings();

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
