#include <Editor/Windows/InspectorComponents/AnimationComponent.hpp>

#include <Editor/UndoStack.hpp>
#include <Mariana/Animation/Animator.hpp>
#include <Mariana/Importers/GLTFLoader.hpp>
#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
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
        case AnimationNodeType::Blend2D:  return "Blend 2D";
        case AnimationNodeType::Additive: return "Additive";
        case AnimationNodeType::Layer:    return "Layer";
        case AnimationNodeType::StateMachine: return "State Machine";
        case AnimationNodeType::TwoBoneIK: return "Two Bone IK";
        case AnimationNodeType::LookAt: return "Look At";
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

    // Adds the animations of a glTF file to the Animator's own clips, under names not taken yet.
    void AddClips(Animator& animator, AssetManager2& assets, const char* file)
    {
        for (NamedClip& clip : GLTF::GLTFLoader::LoadClips(assets, file))
        {
            const bool present = std::any_of(animator.clips.begin(), animator.clips.end(), [&](const NamedClip& c) { return c.clip.GetID() == clip.clip.GetID(); });
            if (present || !clip.clip.Get())
                continue;
            std::string name = clip.name;
            for (int n = 2; animator.FindClip(name) >= 0; ++n)
                name = clip.name + " " + std::to_string(n);
            animator.clips.push_back({ std::move(name), std::move(clip.clip) });
        }
    }

    void DrawGraphPicker(Animator& animator, AssetManager2& assets)
    {
        const AssetMetadata* info = animator.graph.GetMetadata();
        const char* current = animator.graph.Get() ? (info ? info->name.c_str() : "(unnamed)") : "None: clips play directly";
        ImGui::SetNextItemWidth(-ImGui::GetFontSize() * 4.0f);
        if (ImGui::BeginCombo("Graph", current))
        {
            if (ImGui::Selectable("None", !animator.graph.Get()))
                animator.graph = {};
            int row = 0;
            assets.ForEach<AnimationGraph>([&](AssetID id, AnimationGraph&) {
                const AssetMetadata* meta = assets.GetMetadata(id);
                ImGui::PushID(row++);
                if (ImGui::Selectable(meta ? meta->name.c_str() : "(unnamed)", animator.graph.GetID() == id))
                    animator.graph = assets.LoadAsset<AnimationGraph>(id);
                ImGui::PopID();
            });
            ImGui::EndCombo();
        }
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MARIANA_ASSET_ANIMGRAPH"))
            {
                AssetHandle<AnimationGraph> graph = assets.Load<AnimationGraph>(static_cast<const char*>(payload->Data));
                if (graph.Get())
                    animator.graph = graph;
            }
            ImGui::EndDragDropTarget();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Drop an .animgraph file from the Assets window here");
    }

    // Root motion: on/off, the bone it comes from (automatic, or one of the skeleton's), and what it
    // does as the game runs.
    void DrawRootMotion(Animator& animator)
    {
        ImGui::Checkbox("Root Motion", &animator.rootMotion);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("The root bone's walk and turn move the entity (or its physics character)\ninstead of the hips leaving it behind");
        if (!animator.rootMotion)
            return;
        const Skeleton* skeleton = animator.skeleton.Get().get();
        const int bound = animator.RootBone();
        const std::string found = skeleton && bound >= 0 ? std::string(Skeleton::BaseName(skeleton->GetBone(bound).name)) : std::string("none found");
        const std::string automatic = "Automatic (" + found + ")";
        ImGui::SetNextItemWidth(-ImGui::GetFontSize() * 6.0f);
        if (ImGui::BeginCombo("Root Bone", animator.rootBone.empty() ? automatic.c_str() : animator.rootBone.c_str()))
        {
            if (ImGui::Selectable(automatic.c_str(), animator.rootBone.empty()))
                animator.rootBone.clear();
            for (size_t b = 0; skeleton && b < skeleton->BoneCount(); ++b)
            {
                ImGui::PushID(static_cast<int>(b));
                const std::string& name = skeleton->GetBone(b).name;
                if (ImGui::Selectable(name.c_str(), animator.rootBone == name))
                    animator.rootBone = name;
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        const glm::vec3 velocity = animator.RootVelocity();
        ImGui::TextDisabled("Moving %.2f m/s, turning %.0f deg/s", glm::length(glm::vec2(velocity.x, velocity.z)), glm::degrees(animator.RootTurnRate()));
    }

    // Where clip `clip` is now: the time of what plays it (the current clip, or a graph node playing
    // it), else 0.
    float ClipTime(const Animator& animator, int clip)
    {
        for (const Animator::ClipState& state : animator.Playing())
            if (state.clip == clip)
                return state.time;
        for (const Animator::NodeState& node : animator.Nodes())
            if (node.clip == clip && node.weight > 0.0f)
                return node.time;
        return 0.0f;
    }

    // The events of the Animator's own clips; each edit (a drag, typing) is one undo step.
    void DrawEvents(Entity entity, Animator& animator, UndoStack& undo)
    {
        struct Pending
        {
            uint64_t entity = 0;
            int clip = -1;
            std::vector<AnimationEvent> before;
            bool open = false;
        };
        static Pending pending;

        for (int i = 0; i < static_cast<int>(animator.clips.size()); ++i)
        {
            NamedClip& named = animator.clips[i];
            const AnimationClip* clip = named.clip.Get().get();
            ImGui::PushID(i);
            const std::string label = named.name + " (" + std::to_string(named.events.size()) + ")";
            if (ImGui::TreeNodeEx("##events", ImGuiTreeNodeFlags_SpanAvailWidth, "%s", label.c_str()))
            {
                const std::vector<AnimationEvent> before = named.events;
                if (InspectorComponents::DrawClipEvents(named.events, clip ? clip->GetDuration() : 0.0f, ClipTime(animator, i)) &&
                    !(pending.open && pending.entity == entity.RawId() && pending.clip == i))
                    pending = { entity.RawId(), i, before, true };
                ImGui::TreePop();
            }
            if (pending.open && pending.entity == entity.RawId() && pending.clip == i && !ImGui::IsAnyItemActive())
            {
                pending.open = false;
                const std::vector<AnimationEvent> old = pending.before, now = named.events;
                const std::string name = named.name;
                auto set = [entity, i, name](const std::vector<AnimationEvent>& events) mutable {
                    if (Animator* a = entity.IsValid() ? entity.Get<Animator>() : nullptr)
                        if (i < static_cast<int>(a->clips.size()) && a->clips[i].name == name)
                            a->clips[i].events = events;
                };
                undo.Push("Edit events of " + name, [set, old]() mutable { set(old); }, [set, now]() mutable { set(now); });
            }
            ImGui::PopID();
        }
        if (animator.ClipCount() > static_cast<int>(animator.clips.size()))
            ImGui::TextDisabled("The graph's own clips have their events in the graph (Animation Graph window).");
    }

    std::string ClipName(const Animator& animator, int clip)
    {
        const NamedClip* named = animator.GetClip(clip);
        if (!named)
            return "None";
        return named->clip.Get() ? named->name : named->name + " <unloaded>";
    }

    void DrawClips(Animator& animator, AssetManager2& assets, bool hasGraph)
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

        int removed = -1;
        const int own = static_cast<int>(animator.clips.size());
        for (int i = 0; i < animator.ClipCount(); ++i)
        {
            ImGui::PushID(i);
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            const NamedClip& named = *animator.GetClip(i);
            const std::shared_ptr<AnimationClip>& clip = named.clip.Get();
            if (i < own)
            {
                // The name graphs use for it.
                char name[128] = {};
                std::strncpy(name, named.name.c_str(), sizeof(name) - 1);
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::InputText("##name", name, sizeof(name), ImGuiInputTextFlags_EnterReturnsTrue) && name[0])
                    animator.clips[i].name = name;
            }
            else
            {
                ImGui::TextUnformatted(ClipName(animator, i).c_str());
                ImGui::SameLine();
                ImGui::TextDisabled("(graph)");
            }
            if (clip && ImGui::IsItemHovered())
            {
                const AssetMetadata* meta = named.clip.GetMetadata();
                ImGui::SetTooltip("%s\n%.3f s, %zu tracks", meta ? meta->name.c_str() : "", clip->GetDuration(), clip->tracks.size());
            }

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
            ImGui::BeginDisabled(hasGraph || i >= own);
            if (ImGui::SmallButton("Play"))
                animator.Play(i, animator.Loop());
            ImGui::SameLine();
            if (ImGui::SmallButton("Fade"))
                animator.CrossFade(i, fadeSeconds, animator.Loop());
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(i >= own);
            if (ImGui::SmallButton("x"))
                removed = i;
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        ImGui::EndTable();

        if (removed >= 0)
        {
            animator.clips.erase(animator.clips.begin() + removed);
            if (!animator.clips.empty() && animator.CurrentClip() >= static_cast<int>(animator.clips.size()))
                animator.Play(0);
        }

        // Drop zone: a glTF file's animations become clips (a clip library binds by bone name).
        ImGui::Button("Drop a .glb/.gltf here to add its animations", ImVec2(-FLT_MIN, 0.0f));
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MARIANA_ASSET_GLTF"))
                AddClips(animator, assets, static_cast<const char*>(payload->Data));
            ImGui::EndDragDropTarget();
        }
    }

    // Shows a state of each state machine at a time picked here, without playing.
    void DrawPreview(Animator& animator, const AnimationGraph& graph)
    {
        static std::vector<std::pair<int, float>> picked;
        if (picked.size() < graph.nodes.size())
            picked.resize(graph.nodes.size(), { 0, 0.0f });

        bool any = false;
        for (size_t i = 0; i < graph.nodes.size(); ++i)
        {
            const AnimationNode& node = graph.nodes[i];
            if (node.type != AnimationNodeType::StateMachine || node.states.empty())
                continue;
            if (!any)
                ImGui::SeparatorText("Preview");
            any = true;

            int& state = picked[i].first;
            float& time = picked[i].second;
            state = std::clamp(state, 0, static_cast<int>(node.states.size()) - 1);

            ImGui::PushID(static_cast<int>(i));
            bool changed = false;
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.4f);
            if (ImGui::BeginCombo("##state", node.states[state].c_str()))
            {
                for (int s = 0; s < static_cast<int>(node.states.size()); ++s)
                {
                    if (ImGui::Selectable(node.states[s].c_str(), s == state))
                    {
                        state = s;
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-ImGui::GetFontSize() * 6.0f);
            changed |= ImGui::SliderFloat("##time", &time, 0.0f, 1.0f, "%.3f");
            ImGui::SameLine();
            ImGui::TextUnformatted(node.name.c_str());
            if (changed)
                animator.PreviewState(node.states[state], time);
            ImGui::PopID();
        }
        if (any)
            ImGui::TextDisabled("Holds while the game is stopped or paused");
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
            case AnimationParameterType::Vector:
            {
                glm::vec3 vector = animator.GetVectorParameter(i);
                if (ImGui::DragFloat3(parameter.name.c_str(), &vector.x, 0.05f))
                    animator.SetVectorParameter(i, vector);
                break;
            }
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
    bool DrawClipEvents(std::vector<AnimationEvent>& events, float duration, float now)
    {
        bool changed = false;
        // The strip: the clip's length, a mark per event (its name on hover), the time now.
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const float width = std::max(ImGui::GetContentRegionAvail().x, 40.0f);
        const float height = ImGui::GetFrameHeight();
        ImGui::InvisibleButton("##strip", ImVec2(width, height));
        const bool hovered = ImGui::IsItemHovered();
        draw->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + height), IM_COL32(30, 30, 36, 255), 3.0f);
        const auto xOf = [&](float t) { return origin.x + (duration > 0.0f ? std::clamp(t / duration, 0.0f, 1.0f) : 0.0f) * width; };
        const AnimationEvent* under = nullptr;
        for (const AnimationEvent& event : events)
        {
            const float x = xOf(event.time);
            draw->AddTriangleFilled(ImVec2(x - 4.0f, origin.y + 2.0f), ImVec2(x + 4.0f, origin.y + 2.0f), ImVec2(x, origin.y + 9.0f), IM_COL32(255, 170, 60, 255));
            draw->AddLine(ImVec2(x, origin.y + 2.0f), ImVec2(x, origin.y + height - 2.0f), IM_COL32(255, 170, 60, 200));
            if (hovered && std::abs(ImGui::GetMousePos().x - x) < 5.0f)
                under = &event;
        }
        const float nowX = xOf(now);
        draw->AddLine(ImVec2(nowX, origin.y), ImVec2(nowX, origin.y + height), IM_COL32(120, 200, 255, 255), 2.0f);
        if (under)
            ImGui::SetTooltip("%s at %.3f s", under->name.c_str(), under->time);
        else if (hovered)
            ImGui::SetTooltip("%.3f s long; now at %.3f s", duration, now);

        int removed = -1;
        for (int i = 0; i < static_cast<int>(events.size()); ++i)
        {
            ImGui::PushID(i);
            char name[64] = {};
            std::strncpy(name, events[i].name.c_str(), sizeof(name) - 1);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
            if (ImGui::InputText("##name", name, sizeof(name)))
            {
                events[i].name = name;
                changed = true;
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() * 1.5f);
            changed |= ImGui::DragFloat("##time", &events[i].time, 0.005f, 0.0f, std::max(duration, 0.0f), "%.3f s", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SameLine();
            if (ImGui::SmallButton("x"))
                removed = i;
            ImGui::PopID();
        }
        if (removed >= 0)
        {
            events.erase(events.begin() + removed);
            changed = true;
        }
        char add[48];
        std::snprintf(add, sizeof(add), "Add at %.3f s", now);
        if (ImGui::SmallButton(add))
        {
            events.push_back({ std::clamp(now, 0.0f, std::max(duration, 0.0f)), "Event" });
            std::stable_sort(events.begin(), events.end(), [](const AnimationEvent& a, const AnimationEvent& b) { return a.time < b.time; });
            changed = true;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Scripts on the entity hear it in OnAnimationEvent(self, name, clip)");
        return changed;
    }

    void DrawAnimationComponent(Entity& entity, AssetManager2& assets, UndoStack& undo, const AnimatorActions& actions)
    {
        if (!entity.Has<Animator>())
            return;

        Animator& animator = *entity.Get<Animator>();
        if (!ImGui::CollapsingHeader("Animator", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        ImGui::PushID("Animator");
        DrawGraphPicker(animator, assets);
        const AnimationGraph* graph = animator.graph.Get().get();
        if (graph && actions.editGraph)
        {
            if (ImGui::SmallButton("Edit graph"))
                actions.editGraph(animator.graph);
            ImGui::SameLine();
        }
        if (actions.newGraph && ImGui::SmallButton("New graph"))
            actions.newGraph(entity);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Makes a state machine with a state per clip, saved in the project's Animations folder");
        if (graph && !animator.GraphError().empty())
            ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.3f, 1.0f), "Graph not used yet: %s", animator.GraphError().c_str());

        ImGui::Checkbox("Playing", &animator.playing);
        if (!graph)
        {
            ImGui::SameLine();
            bool loop = animator.Loop();
            if (ImGui::Checkbox("Loop", &loop))
                animator.SetLoop(loop);
        }
        ImGui::DragFloat("Speed", &animator.speed, 0.01f, -10.0f, 10.0f, "%.2f");
        DrawRootMotion(animator);

        if (!graph)
        {
            const std::shared_ptr<AnimationClip>& clip = animator.GetCurrentClip();
            const float duration = clip ? clip->GetDuration() : 0.0f;
            float time = animator.Time();
            if (ImGui::SliderFloat("Time", &time, 0.0f, duration, "%.3f"))
                animator.SetTime(time);
        }

        ImGui::SeparatorText("Clips");
        DrawClips(animator, assets, graph != nullptr);
        ImGui::SeparatorText("Events");
        DrawEvents(entity, animator, undo);

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
            DrawPreview(animator, *graph);
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
