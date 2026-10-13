#include <Editor/Windows/InspectorComponents/AnimationComponent.hpp>

#include <Mariana/Animation/AnimationPlayer.hpp>
#include <imgui.h>

#include <string>

namespace InspectorComponents
{
    void DrawAnimationComponent(Entity& entity)
    {
        if (!entity.Has<AnimationPlayer>())
            return;

        AnimationPlayer& player = *entity.Get<AnimationPlayer>();
        if (ImGui::CollapsingHeader("Animation Player", ImGuiTreeNodeFlags_DefaultOpen))
        {
            std::shared_ptr<Animation> clip = player.GetCurrentClip();

            ImGui::PushID("AnimationPlayer");
            if (ImGui::BeginCombo("Clip", clip ? clip->GetName().c_str() : "None"))
            {
                for (int i = 0; i < static_cast<int>(player.clips.size()); ++i)
                {
                    std::shared_ptr<Animation> option = player.clips[i].Get();
                    const std::string label = option ? option->GetName() : "<unloaded>";

                    ImGui::PushID(i);
                    if (ImGui::Selectable(label.c_str(), i == player.currentClip))
                        player.Play(i);
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }

            ImGui::Checkbox("Playing", &player.playing);
            ImGui::SameLine();
            ImGui::Checkbox("Loop", &player.loop);
            ImGui::DragFloat("Speed", &player.speed, 0.01f, -10.0f, 10.0f, "%.2f");

            const float duration = clip ? clip->GetDuration() : 0.0f;
            ImGui::SliderFloat("Time", &player.time, 0.0f, duration, "%.3f");
            ImGui::PopID();

            ImGui::TextDisabled("Duration");
            ImGui::SameLine(0, 16);
            ImGui::Text("%.3f", duration);

            ImGui::TextDisabled("Channels");
            ImGui::SameLine(0, 16);
            ImGui::Text("%zu", clip ? clip->GetChannelCount() : 0);

            ImGui::TextDisabled("Node Count");
            ImGui::SameLine(0, 16);
            ImGui::Text("%d", clip ? clip->GetNodeCount() : 0);
        }
    }
}
