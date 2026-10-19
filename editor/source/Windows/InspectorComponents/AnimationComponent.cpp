#include <Editor/Windows/InspectorComponents/AnimationComponent.hpp>

#include <Mariana/Animation/Animator.hpp>
#include <imgui.h>

#include <cstdio>
#include <string>

namespace
{
    const char* NodeTypeName(AnimationNodeType type)
    {
        switch (type)
        {
        case AnimationNodeType::Clip:     return "Clip";
        case AnimationNodeType::Blend:    return "Blend";
        case AnimationNodeType::Blend1D:  return "Blend 1D";
        case AnimationNodeType::Additive: return "Additive";
        case AnimationNodeType::Layer:    return "Layer";
        case AnimationNodeType::StateMachine: return "State Machine";
        }
        return "?";
    }

    void WeightBar(float weight, const char* text = nullptr)
    {
        char overlay[32];
        if (!text)
        {
            std::snprintf(overlay, sizeof(overlay), "%.2f", weight);
            text = overlay;
        }
        ImGui::ProgressBar(weight, ImVec2(-FLT_MIN, 0.0f), text);
    }

    std::string ClipName(const Animator& animator, int clip)
    {
        if (clip < 0 || clip >= static_cast<int>(animator.clips.size()))
            return "None";
        const std::shared_ptr<AnimationClip>& asset = animator.clips[clip].Get();
        return asset ? asset->GetName() : "<unloaded>";
    }

    void DrawClips(Animator& animator, bool hasGraph)
    {
        static float fadeSeconds = 0.3f;
        if (!hasGraph)
        {
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6.0f);
            ImGui::DragFloat("Cross-fade", &fadeSeconds, 0.01f, 0.0f, 10.0f, "%.2f s");
        }

        if (!ImGui::BeginTable("Clips", 3, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg))
            return;
        ImGui::TableSetupColumn("Clip", ImGuiTableColumnFlags_WidthStretch, 3.0f);
        ImGui::TableSetupColumn("Weight", ImGuiTableColumnFlags_WidthStretch, 1.5f);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed);

        for (int i = 0; i < static_cast<int>(animator.clips.size()); ++i)
        {
            ImGui::PushID(i);
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            const std::shared_ptr<AnimationClip>& clip = animator.clips[i].Get();
            ImGui::TextUnformatted(ClipName(animator, i).c_str());
            if (clip && ImGui::IsItemHovered())
                ImGui::SetTooltip("%.3f s, %zu tracks", clip->GetDuration(), clip->tracks.size());

            ImGui::TableNextColumn();
            float weight = 0.0f;
            if (hasGraph)
            {
                for (const Animator::NodeState& node : animator.Nodes())
                    if (node.clip == i)
                        weight += node.weight;
            }
            else
            {
                for (const Animator::ClipState& state : animator.Playing())
                    if (state.clip == i)
                        weight += state.weight;
            }
            WeightBar(weight);

            ImGui::TableNextColumn();
            ImGui::BeginDisabled(hasGraph);
            if (ImGui::SmallButton("Play"))
                animator.Play(i, animator.Loop());
            ImGui::SameLine();
            if (ImGui::SmallButton("Fade"))
                animator.CrossFade(i, fadeSeconds, animator.Loop());
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    void DrawParameters(Animator& animator, const AnimationGraph& graph)
    {
        if (graph.parameters.empty())
            return;
        ImGui::SeparatorText("Parameters");
        for (int i = 0; i < static_cast<int>(graph.parameters.size()); ++i)
        {
            const AnimationParameter& parameter = graph.parameters[i];
            ImGui::PushID(i);
            float value = animator.GetParameter(i);
            switch (parameter.type)
            {
            case AnimationParameterType::Float:
                if (ImGui::DragFloat(parameter.name.c_str(), &value, 0.01f))
                    animator.SetParameter(i, value);
                break;
            case AnimationParameterType::Bool:
            {
                bool on = value != 0.0f;
                if (ImGui::Checkbox(parameter.name.c_str(), &on))
                    animator.SetParameter(i, on ? 1.0f : 0.0f);
                break;
            }
            case AnimationParameterType::Trigger:
                if (ImGui::Button(parameter.name.c_str()))
                    animator.SetParameter(i, 1.0f);
                ImGui::SameLine();
                ImGui::TextDisabled("%s", value != 0.0f ? "set" : "trigger");
                break;
            }
            ImGui::PopID();
        }
    }

    // What a state machine is doing: its state, or the transition under way with how far it got.
    std::string MachineText(const Animator& animator, const AnimationNode& node, int index)
    {
        if (index >= static_cast<int>(animator.Machines().size()) || animator.Machines()[index].active.empty())
            return {};
        const std::vector<Animator::ActiveState>& active = animator.Machines()[index].active;
        auto name = [&](int state) { return state >= 0 && state < static_cast<int>(node.states.size()) ? node.states[state] : std::string("?"); };
        const Animator::ActiveState& current = active.back();
        if (active.size() == 1 || current.fade >= 1.0f)
            return name(current.state);
        char progress[16];
        std::snprintf(progress, sizeof(progress), " %d%%", static_cast<int>(current.fade * 100.0f + 0.5f));
        return name(active[active.size() - 2].state) + " -> " + name(current.state) + progress;
    }

    void DrawNode(const Animator& animator, const AnimationGraph& graph, int index, int depth, const char* label = nullptr)
    {
        if (index < 0 || index >= static_cast<int>(graph.nodes.size()) || depth > 32)
            return;
        const AnimationNode& node = graph.nodes[index];
        const Animator::NodeState* state = index < static_cast<int>(animator.Nodes().size()) ? &animator.Nodes()[index] : nullptr;

        ImGui::PushID(index);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        const bool leaf = node.inputs.empty();
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth;
        if (leaf)
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        const bool open = label ? ImGui::TreeNodeEx("##node", flags, "%s: %s", label, node.name.c_str())
                                : ImGui::TreeNodeEx("##node", flags, "%s", node.name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("%s", NodeTypeName(node.type));

        ImGui::TableNextColumn();
        char text[64] = {};
        if (state && node.type == AnimationNodeType::Clip)
            std::snprintf(text, sizeof(text), "%.2f  @ %.2f s", state->weight, state->time);
        if (node.type == AnimationNodeType::StateMachine)
            std::snprintf(text, sizeof(text), "%s", MachineText(animator, node, index).c_str());
        WeightBar(state ? state->weight : 0.0f, text[0] ? text : nullptr);

        if (open && !leaf)
        {
            for (size_t i = 0; i < node.inputs.size(); ++i)
            {
                const bool isState = node.type == AnimationNodeType::StateMachine && i < node.states.size();
                DrawNode(animator, graph, node.inputs[i], depth + 1, isState ? node.states[i].c_str() : nullptr);
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

namespace InspectorComponents
{
    void DrawAnimationComponent(Entity& entity)
    {
        if (!entity.Has<Animator>())
            return;

        Animator& animator = *entity.Get<Animator>();
        if (!ImGui::CollapsingHeader("Animator", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        ImGui::PushID("Animator");
        const AnimationGraph* graph = animator.graph.Get().get();
        const AssetMetadata* graphInfo = animator.graph.GetMetadata();
        ImGui::TextDisabled("Graph");
        ImGui::SameLine(0, 16);
        ImGui::TextUnformatted(graph ? (graphInfo ? graphInfo->name.c_str() : "(unnamed)") : "None: clips play directly");

        ImGui::Checkbox("Playing", &animator.playing);
        if (!graph)
        {
            ImGui::SameLine();
            bool loop = animator.Loop();
            if (ImGui::Checkbox("Loop", &loop))
                animator.SetLoop(loop);
        }
        ImGui::DragFloat("Speed", &animator.speed, 0.01f, -10.0f, 10.0f, "%.2f");

        if (!graph)
        {
            const std::shared_ptr<AnimationClip>& clip = animator.GetCurrentClip();
            const float duration = clip ? clip->GetDuration() : 0.0f;
            float time = animator.Time();
            if (ImGui::SliderFloat("Time", &time, 0.0f, duration, "%.3f"))
                animator.SetTime(time);
        }

        ImGui::SeparatorText("Clips");
        DrawClips(animator, graph != nullptr);

        if (graph)
        {
            DrawParameters(animator, *graph);
            ImGui::SeparatorText("Nodes");
            if (ImGui::BeginTable("Nodes", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg))
            {
                ImGui::TableSetupColumn("Node", ImGuiTableColumnFlags_WidthStretch, 3.0f);
                ImGui::TableSetupColumn("Weight", ImGuiTableColumnFlags_WidthStretch, 2.0f);
                DrawNode(animator, *graph, graph->output, 0);
                ImGui::EndTable();
            }
        }

        ImGui::SeparatorText("Binding");
        const Skeleton* skeleton = animator.skeleton.Get().get();
        auto row = [](const char* label, const std::string& value) {
            ImGui::TextDisabled("%s", label);
            ImGui::SameLine(0, 16);
            ImGui::TextUnformatted(value.c_str());
        };
        const std::shared_ptr<AnimationClip>& current = animator.GetCurrentClip();
        if (!graph)
            row("Tracks", current ? std::to_string(animator.BoundTracks()) + " of " + std::to_string(current->tracks.size()) + " bound" : "-");
        row("Bones", skeleton ? std::to_string(animator.BoundBones()) + " of " + std::to_string(skeleton->BoneCount()) + " found" : "No skeleton");
        ImGui::PopID();
    }
}
