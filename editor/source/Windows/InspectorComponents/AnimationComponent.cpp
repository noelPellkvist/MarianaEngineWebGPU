#include <Editor/Windows/InspectorComponents/AnimationComponent.hpp>

#include <Mariana/Animation/Animator.hpp>
#include <imgui.h>

#include <cstdio>
#include <string>

namespace InspectorComponents
{
    void DrawAnimationComponent(Entity& entity)
    {
        if (!entity.Has<Animator>())
            return;

        Animator& animator = *entity.Get<Animator>();
        if (ImGui::CollapsingHeader("Animator", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const std::shared_ptr<AnimationClip>& clip = animator.GetCurrentClip();

            ImGui::PushID("Animator");
            if (ImGui::BeginCombo("Clip", clip ? clip->GetName().c_str() : "None"))
            {
                for (int i = 0; i < static_cast<int>(animator.clips.size()); ++i)
                {
                    const std::shared_ptr<AnimationClip>& option = animator.clips[i].Get();
                    const std::string label = option ? option->GetName() : "<unloaded>";

                    ImGui::PushID(i);
                    if (ImGui::Selectable(label.c_str(), i == animator.CurrentClip()))
                        animator.Play(i, animator.Loop());
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }

            ImGui::Checkbox("Playing", &animator.playing);
            ImGui::SameLine();
            bool loop = animator.Loop();
            if (ImGui::Checkbox("Loop", &loop))
                animator.SetLoop(loop);
            ImGui::DragFloat("Speed", &animator.speed, 0.01f, -10.0f, 10.0f, "%.2f");

            const float duration = clip ? clip->GetDuration() : 0.0f;
            float time = animator.Time();
            if (ImGui::SliderFloat("Time", &time, 0.0f, duration, "%.3f"))
                animator.SetTime(time);
            ImGui::PopID();

            const Skeleton* skeleton = animator.skeleton.Get().get();
            auto row = [](const char* label, const std::string& value) {
                ImGui::TextDisabled("%s", label);
                ImGui::SameLine(0, 16);
                ImGui::TextUnformatted(value.c_str());
            };
            char seconds[32];
            std::snprintf(seconds, sizeof(seconds), "%.3f s", duration);
            row("Duration", seconds);
            row("Tracks", clip ? std::to_string(animator.BoundTracks()) + " of " + std::to_string(clip->tracks.size()) + " bound" : "-");
            row("Bones", skeleton ? std::to_string(animator.BoundBones()) + " of " + std::to_string(skeleton->BoneCount()) + " found" : "No skeleton");
        }
    }
}
