#include "Editor/Windows/InspectorComponents/AnimationComponent.hpp"

#include <AnimationPlayer.hpp>
#include <imgui.h>

namespace
{
    const char* BoolText(bool value)
    {
        return value ? "Yes" : "No";
    }
}

namespace InspectorComponents
{
    void DrawAnimationComponent(Entity& entity)
    {
        if (!entity.Has<AnimationPlayer>())
            return;

        AnimationPlayer& player = *entity.Get<AnimationPlayer>();
        if (ImGui::CollapsingHeader("Animation Player", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TextDisabled("Current Animation");
            ImGui::SameLine(0, 16);
            if (player.m_CurrentAnimation)
                ImGui::TextUnformatted(player.m_CurrentAnimation->GetName().c_str());
            else
                ImGui::TextDisabled("None");

            ImGui::TextDisabled("Time");
            ImGui::SameLine(0, 16);
            ImGui::Text("%.3f", player.m_Time);

            ImGui::TextDisabled("Duration");
            ImGui::SameLine(0, 16);
            ImGui::Text("%.3f", player.m_CurrentAnimation ? player.m_CurrentAnimation->GetDuration() : 0.0f);

            ImGui::TextDisabled("Channels");
            ImGui::SameLine(0, 16);
            ImGui::Text("%zu", player.m_CurrentAnimation ? player.m_CurrentAnimation->GetChannelCount() : 0);

            ImGui::TextDisabled("Node Count");
            ImGui::SameLine(0, 16);
            ImGui::Text("%d", player.m_CurrentAnimation ? player.m_CurrentAnimation->GetNodeCount() : 0);

            ImGui::TextDisabled("Root Valid");
            ImGui::SameLine(0, 16);
            ImGui::TextUnformatted(BoolText(player.m_Root.IsValid()));
        }
    }
}