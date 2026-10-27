#include <Editor/Windows/AnimationGraphWindow.hpp>

#include <Editor/UndoStack.hpp>
#include <Editor/Windows/InspectorComponents/AnimationComponent.hpp>
#include <Mariana/Animation/Animator.hpp>
#include <Mariana/Foundation/Logger.hpp>
#include <Mariana/Importers/AnimationGraphFile.hpp>
#include <Mariana/Importers/GLTFLoader.hpp>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <functional>

namespace
{
    constexpr float kNodeWidth = 190.0f;
    constexpr float kHeader = 42.0f; // title and subtitle
    constexpr float kRow = 22.0f;
    constexpr float kPinRadius = 5.0f;
    constexpr float kPinHit = 9.0f; // pixels
    const glm::vec2 kStateSize(150.0f, 46.0f);

    const char* const kTypeNames[] = { "Clip", "Blend", "Blend 1D", "Blend 2D", "Additive", "Layer", "State Machine", "Two Bone IK", "Look At" };
    constexpr int kTypeCount = static_cast<int>(sizeof(kTypeNames) / sizeof(kTypeNames[0]));
    const char* const kModeNames[] = { "is true", "is false", ">", "<", "==", "!=" };
    const char* const kParameterTypes[] = { "Float", "Bool", "Trigger", "Vector" };

    const char* TypeName(AnimationNodeType type)
    {
        return kTypeNames[static_cast<int>(type)];
    }

    ImU32 TypeColor(AnimationNodeType type)
    {
        switch (type)
        {
        case AnimationNodeType::Clip:         return IM_COL32(58, 98, 156, 255);
        case AnimationNodeType::Blend:        return IM_COL32(150, 104, 52, 255);
        case AnimationNodeType::Blend1D:      return IM_COL32(160, 118, 44, 255);
        case AnimationNodeType::Blend2D:      return IM_COL32(168, 92, 52, 255);
        case AnimationNodeType::Additive:     return IM_COL32(72, 128, 84, 255);
        case AnimationNodeType::Layer:        return IM_COL32(104, 82, 156, 255);
        case AnimationNodeType::StateMachine: return IM_COL32(150, 62, 88, 255);
        case AnimationNodeType::TwoBoneIK:    return IM_COL32(44, 132, 140, 255);
        case AnimationNodeType::LookAt:       return IM_COL32(52, 140, 118, 255);
        }
        return IM_COL32(90, 90, 90, 255);
    }

    std::string Format(const char* format, ...)
    {
        char buffer[256];
        va_list args;
        va_start(args, format);
        std::vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        return buffer;
    }

    std::string ParameterName(const AnimationGraph& graph, int parameter)
    {
        return parameter >= 0 && parameter < static_cast<int>(graph.parameters.size()) ? graph.parameters[parameter].name : std::string("(none)");
    }

    std::string WeightText(const AnimationGraph& graph, const AnimationWeight& weight)
    {
        return weight.parameter >= 0 ? ParameterName(graph, weight.parameter) : Format("%.2f", weight.value);
    }

    std::string Subtitle(const AnimationGraph& graph, const AnimationNode& node)
    {
        switch (node.type)
        {
        case AnimationNodeType::Clip:
            return (node.clip.empty() ? std::string("(no clip)") : node.clip) + (node.speed != 1.0f ? Format("  x%.2f", node.speed) : std::string());
        case AnimationNodeType::Blend:
        case AnimationNodeType::Additive:
            return "weight " + WeightText(graph, node.weight);
        case AnimationNodeType::Blend1D:
            return "on " + ParameterName(graph, node.parameter);
        case AnimationNodeType::Blend2D:
            return "on " + ParameterName(graph, node.parameter) + ", " + ParameterName(graph, node.parameterY);
        case AnimationNodeType::Layer:
            return std::string(node.layerMode == LayerMode::Additive ? "additive" : "override") +
                   (node.mask.entries.empty() ? std::string(", all bones") : ", from " + node.mask.entries[0].bone);
        case AnimationNodeType::StateMachine:
            return Format("%zu states", node.states.size());
        case AnimationNodeType::TwoBoneIK:
            return (node.bone.empty() ? std::string("(no bone)") : node.bone) + " to " + ParameterName(graph, node.parameter);
        case AnimationNodeType::LookAt:
            return (node.bone.empty() ? std::string("(no bone)") : node.bone) + " at " + ParameterName(graph, node.parameter);
        }
        return {};
    }

    std::string InputLabel(const AnimationNode& node, size_t k)
    {
        switch (node.type)
        {
        case AnimationNodeType::Blend:        return k == 0 ? "A" : "B";
        case AnimationNodeType::Blend1D:      return k < node.thresholds.size() ? Format("at %.2f", node.thresholds[k]) : "?";
        case AnimationNodeType::Blend2D:      return k < node.positions.size() ? Format("(%.2f, %.2f)", node.positions[k].x, node.positions[k].y) : "?";
        case AnimationNodeType::Additive:     return k == 0 ? "Base" : "Additive";
        case AnimationNodeType::Layer:        return k == 0 ? "Base" : "Layer";
        case AnimationNodeType::StateMachine: return k < node.states.size() ? node.states[k] : "?";
        case AnimationNodeType::TwoBoneIK:
        case AnimationNodeType::LookAt:       return "Pose";
        default:                              return {};
        }
    }

    // Whether `from` is `to` or feeds it (connecting to's output into from would make a loop).
    bool Feeds(const AnimationGraph& graph, int from, int to)
    {
        std::vector<bool> seen(graph.nodes.size(), false);
        std::function<bool(int)> visit = [&](int n) {
            if (n < 0 || n >= static_cast<int>(graph.nodes.size()) || seen[n])
                return false;
            if (n == from)
                return true;
            seen[n] = true;
            for (int input : graph.nodes[n].inputs)
                if (visit(input))
                    return true;
            return false;
        };
        return visit(to);
    }

    // Where the line from the middle of a box toward `toward` leaves it.
    ImVec2 BoxEdge(ImVec2 min, ImVec2 max, ImVec2 toward)
    {
        const ImVec2 c((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
        const float dx = toward.x - c.x, dy = toward.y - c.y;
        const float hx = (max.x - min.x) * 0.5f, hy = (max.y - min.y) * 0.5f;
        if (dx == 0.0f && dy == 0.0f)
            return c;
        const float t = std::min(dx != 0.0f ? hx / std::abs(dx) : 1e9f, dy != 0.0f ? hy / std::abs(dy) : 1e9f);
        return ImVec2(c.x + dx * t, c.y + dy * t);
    }

    bool InputString(const char* label, std::string& value)
    {
        char buffer[256] = {};
        std::strncpy(buffer, value.c_str(), sizeof(buffer) - 1);
        if (!ImGui::InputText(label, buffer, sizeof(buffer)))
            return false;
        value = buffer;
        return true;
    }

    std::filesystem::path UniqueFile(std::filesystem::path file)
    {
        std::error_code ec;
        const std::string stem = file.stem().string();
        for (int n = 2; std::filesystem::exists(file, ec); ++n)
            file = file.parent_path() / (stem + " " + std::to_string(n) + file.extension().string());
        return file;
    }
}

AnimationGraphWindow::AnimationGraphWindow(GUI& gui, AssetManager2& assets, UndoStack& undo, const Scene& scene, std::filesystem::path projectDir)
    : EditorWindow("Animation Graph", gui, false), m_Assets(assets), m_Undo(undo), m_Scene(scene), m_ProjectDir(std::move(projectDir))
{
}

void AnimationGraphWindow::Open(AssetHandle<AnimationGraph> graph)
{
    CommitIfIdle(true);
    m_Graph = std::move(graph);
    m_SavedRevision = m_Graph.Get() ? m_Graph.Get()->Revision() : 0;
    m_LayoutDirty = false;
    m_View = -1;
    m_SelectedNode = m_SelectedState = m_SelectedTransition = -1;
    m_SelectedAnyState = false;
    m_Drag = DragKind::None;
    m_MachineCanvases.clear();
    m_GraphCanvas = GraphCanvas();
    m_FrameGraph = true;
    m_Status.clear();
    m_open = true;
    ImGui::SetWindowFocus("###AnimationGraph");

    // Graphs written by hand have no layout yet.
    m_NeedsLayout = false;
    if (m_Graph.Get())
    {
        const AnimationGraph& graph = G();
        m_NeedsLayout = graph.nodes.size() > 1 && std::all_of(graph.nodes.begin(), graph.nodes.end(), [](const AnimationNode& n) { return n.position == glm::vec2(0.0f); });
    }
}

AssetHandle<AnimationGraph> AnimationGraphWindow::CreateGraph(std::filesystem::path file, const Animator* animator)
{
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    file = UniqueFile(file);

    // A state machine with a state per clip of the Animator, or nothing.
    AnimationGraph graph;
    if (animator && !animator->clips.empty())
    {
        AnimationNode machine;
        machine.type = AnimationNodeType::StateMachine;
        machine.name = "Body";
        for (size_t i = 0; i < animator->clips.size(); ++i)
        {
            AnimationNode clip;
            clip.type = AnimationNodeType::Clip;
            clip.name = animator->clips[i].name;
            clip.clip = animator->clips[i].name;
            clip.position = glm::vec2(-300.0f, static_cast<float>(i) * 90.0f);
            graph.nodes.push_back(clip);
            machine.AddState(clip.name, static_cast<int>(i), glm::vec2(static_cast<float>(i % 3) * 220.0f, static_cast<float>(i / 3) * 110.0f));
        }
        machine.position = glm::vec2(0.0f, 0.0f);
        graph.output = graph.AddNode(machine);
    }

    std::string error;
    if (!AnimationGraphFile::Save(graph, m_Assets, file, error))
    {
        Logger::Error("Could not make " + file.string() + ": " + error);
        return {};
    }
    AssetHandle<AnimationGraph> handle = m_Assets.Load<AnimationGraph>(file);
    if (handle.Get())
        Open(handle);
    return handle;
}

Animator* AnimationGraphWindow::PreviewAnimator()
{
    if (!m_Graph.Get())
        return nullptr;
    for (Entity e = m_PreviewEntity; e.IsValid(); e = m_Scene.Parent(e))
        if (Animator* animator = e.Get<Animator>(); animator && animator->graph.Get() == m_Graph.Get())
            return animator;
    return nullptr;
}

template <typename Fn>
void AnimationGraphWindow::Apply(const char* name, Fn&& fn)
{
    CommitIfIdle(true);
    const AnimationGraph before = G();
    fn(G());
    G().MarkChanged();
    PushStep(name, before);
}

void AnimationGraphWindow::Begin(const char* name)
{
    if (m_Pending)
        return;
    m_Before = G();
    m_PendingName = name;
    m_Pending = true;
}

void AnimationGraphWindow::Changed()
{
    G().MarkChanged();
}

void AnimationGraphWindow::CommitIfIdle(bool force)
{
    if (!m_Pending || !m_Graph.Get())
        return;
    if (!force && (ImGui::IsAnyItemActive() || m_Drag != DragKind::None))
        return;
    m_Pending = false;
    PushStep(m_PendingName, m_Before);
    m_Before = AnimationGraph();
}

void AnimationGraphWindow::PushStep(const std::string& name, const AnimationGraph& before)
{
    AssetHandle<AnimationGraph> handle = m_Graph;
    const AnimationGraph after = G();
    m_Undo.Push(name,
        [handle, before]() { if (handle.Get()) handle.Get()->ReplaceWith(before); },
        [handle, after]() { if (handle.Get()) handle.Get()->ReplaceWith(after); });
}

bool AnimationGraphWindow::Save()
{
    CommitIfIdle(true);
    const AssetMetadata* meta = m_Graph.GetMetadata();
    if (!meta || meta->path.empty())
    {
        m_Status = "This graph was made in code and has no file";
        return false;
    }
    std::string error;
    if (!AnimationGraphFile::Save(G(), m_Assets, meta->path, error))
    {
        m_Status = error;
        return false;
    }
    m_SavedRevision = G().Revision();
    m_LayoutDirty = false;
    m_Status = error.empty() ? "Saved" : "Saved; " + error;
    return true;
}

void AnimationGraphWindow::Draw()
{
    if (!m_open)
        return;

    const AssetMetadata* meta = m_Graph.GetMetadata();
    const bool dirty = m_Graph.Get() && (G().Revision() != m_SavedRevision || m_LayoutDirty);
    const std::string title = "Animation Graph" + (meta ? " - " + meta->name : std::string()) + (dirty ? " *" : "") + "###AnimationGraph";
    ImGui::SetNextWindowSize(ImVec2(1100.0f, 640.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(title.c_str(), &m_open))
    {
        if (!m_Graph.Get())
        {
            ImGui::TextWrapped("No graph is open. Press Edit or New next to an Animator's graph in the Inspector, "
                               "double-click an .animgraph file in the Assets window, or right-click there to make one.");
        }
        else
        {
            // Undo may have removed what was selected or shown.
            const int count = static_cast<int>(G().nodes.size());
            if (m_SelectedNode >= count) m_SelectedNode = -1;
            if (m_View >= count || (m_View >= 0 && G().nodes[m_View].type != AnimationNodeType::StateMachine)) m_View = -1;
            if (m_View >= 0)
            {
                const AnimationNode& machine = G().nodes[m_View];
                if (m_SelectedState >= static_cast<int>(machine.states.size())) m_SelectedState = -1;
                if (m_SelectedTransition >= static_cast<int>(machine.transitions.size())) m_SelectedTransition = -1;
            }

            if (m_NeedsLayout)
            {
                ArrangeGraph();
                for (int i = 0; i < count; ++i)
                    if (G().nodes[i].type == AnimationNodeType::StateMachine)
                        ArrangeMachine(i);
                m_NeedsLayout = false;
                m_LayoutDirty = true;
            }

            DrawToolbar();
            ImGui::BeginChild("##side", ImVec2(360.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
            DrawSidePanel();
            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("##canvas", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove);
            if (m_View >= 0)
                DrawMachineView(m_View);
            else
                DrawGraphView();
            ImGui::EndChild();

            if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteFocused))
                Save();
            CommitIfIdle();
        }
    }
    ImGui::End();
}

void AnimationGraphWindow::DrawToolbar()
{
    if (ImGui::Button("Save"))
        Save();
    ImGui::SameLine();
    if (ImGui::Button("Arrange"))
    {
        if (m_View >= 0)
        {
            Apply("Arrange states", [&](AnimationGraph&) { ArrangeMachine(m_View); });
            m_MachineCanvases.erase(m_View);
        }
        else
        {
            Apply("Arrange nodes", [&](AnimationGraph&) { ArrangeGraph(); });
            m_FrameGraph = true;
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    if (ImGui::SmallButton("Graph"))
        m_View = -1;
    if (m_View >= 0)
    {
        ImGui::SameLine();
        ImGui::TextDisabled(">");
        ImGui::SameLine();
        ImGui::TextUnformatted(G().nodes[m_View].name.c_str());
    }

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    std::string error;
    if (G().Validate(error))
        ImGui::TextColored(ImVec4(0.45f, 0.8f, 0.45f, 1.0f), "Ready");
    else
        ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.3f, 1.0f), "Not usable yet: %s", error.c_str());

    if (const Animator* animator = PreviewAnimator(); animator && m_PreviewEntity.IsValid())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("| live:");
        ImGui::SameLine();
        ImGui::TextUnformatted(m_PreviewEntity.GetName());
    }
    if (!m_Status.empty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("| %s", m_Status.c_str());
    }
}

void AnimationGraphWindow::DrawSidePanel()
{
    if (ImGui::CollapsingHeader("Selected", ImGuiTreeNodeFlags_DefaultOpen))
        DrawSelection();
    if (ImGui::CollapsingHeader("Parameters", ImGuiTreeNodeFlags_DefaultOpen))
        DrawParameters();
    if (ImGui::CollapsingHeader("Clips of the graph"))
        DrawClips();
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("Drag boxes to move them. Drag from a node's output (right) to another's input (left) to connect them. "
                       "Right-click to add, middle or right drag pans, the wheel zooms, Delete removes. "
                       "Double-click a state machine to see its states; right-click a state to make a transition.");
    ImGui::PopStyleColor();
}

bool AnimationGraphWindow::ParameterCombo(const char* label, int& parameter, bool floatsOnly, bool allowNone)
{
    AnimationGraph& graph = G();
    bool changed = false;
    if (ImGui::BeginCombo(label, ParameterName(graph, parameter).c_str()))
    {
        if (allowNone && ImGui::Selectable("(none)", parameter < 0))
        {
            parameter = -1;
            changed = true;
        }
        for (int i = 0; i < static_cast<int>(graph.parameters.size()); ++i)
        {
            if (floatsOnly && graph.parameters[i].type != AnimationParameterType::Float)
                continue;
            ImGui::PushID(i);
            if (ImGui::Selectable(graph.parameters[i].name.c_str(), parameter == i))
            {
                parameter = i;
                changed = true;
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    return changed;
}

void AnimationGraphWindow::DrawParameters()
{
    AnimationGraph& graph = G();
    Animator* live = PreviewAnimator();
    int removed = -1;
    if (ImGui::BeginTable("##parameters", live ? 5 : 4, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 1.1f);
        ImGui::TableSetupColumn("Default", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        if (live)
            ImGui::TableSetupColumn("Live", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();

        for (int i = 0; i < static_cast<int>(graph.parameters.size()); ++i)
        {
            AnimationParameter& parameter = graph.parameters[i];
            ImGui::PushID(i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            std::string name = parameter.name;
            if (InputString("##name", name))
            {
                Begin("Rename parameter");
                parameter.name = name;
                Changed();
            }

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            int type = static_cast<int>(parameter.type);
            if (ImGui::Combo("##type", &type, kParameterTypes, 4))
                Apply("Change parameter type", [&](AnimationGraph& g) {
                    g.parameters[i].type = static_cast<AnimationParameterType>(type);
                    g.parameters[i].defaultValue = 0.0f;
                    g.parameters[i].defaultVector = glm::vec3(0.0f);
                });

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (parameter.type == AnimationParameterType::Float)
            {
                float value = parameter.defaultValue;
                if (ImGui::DragFloat("##default", &value, 0.01f))
                {
                    Begin("Edit parameter default");
                    parameter.defaultValue = value;
                    Changed();
                }
            }
            else if (parameter.type == AnimationParameterType::Bool)
            {
                bool value = parameter.defaultValue != 0.0f;
                if (ImGui::Checkbox("##default", &value))
                    Apply("Edit parameter default", [&](AnimationGraph& g) { g.parameters[i].defaultValue = value ? 1.0f : 0.0f; });
            }
            else if (parameter.type == AnimationParameterType::Vector)
            {
                glm::vec3 value = parameter.defaultVector;
                if (ImGui::DragFloat3("##default", &value.x, 0.05f))
                {
                    Begin("Edit parameter default");
                    parameter.defaultVector = value;
                    Changed();
                }
            }

            if (live)
            {
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-FLT_MIN);
                float value = live->GetParameter(i);
                if (parameter.type == AnimationParameterType::Vector)
                {
                    glm::vec3 vector = live->GetVectorParameter(i);
                    if (ImGui::DragFloat3("##live", &vector.x, 0.05f))
                        live->SetVectorParameter(i, vector);
                }
                else if (parameter.type == AnimationParameterType::Float)
                {
                    if (ImGui::DragFloat("##live", &value, 0.01f))
                        live->SetParameter(i, value);
                }
                else if (parameter.type == AnimationParameterType::Bool)
                {
                    bool on = value != 0.0f;
                    if (ImGui::Checkbox("##live", &on))
                        live->SetParameter(i, on ? 1.0f : 0.0f);
                }
                else if (ImGui::SmallButton(value != 0.0f ? "set" : "fire"))
                    live->SetParameter(i, 1.0f);
            }

            ImGui::TableNextColumn();
            if (ImGui::SmallButton("x"))
                removed = i;
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (removed >= 0)
        Apply("Remove parameter", [&](AnimationGraph& g) { g.RemoveParameter(removed); });

    auto add = [&](const char* label, AnimationParameterType type, const char* base) {
        if (!ImGui::SmallButton(label))
            return;
        std::string name = base;
        for (int n = 2; graph.FindParameter(name) >= 0; ++n)
            name = std::string(base) + " " + std::to_string(n);
        Apply("Add parameter", [&](AnimationGraph& g) { g.parameters.push_back({ name, type, 0.0f }); });
    };
    add("+ Float", AnimationParameterType::Float, "value");
    ImGui::SameLine();
    add("+ Bool", AnimationParameterType::Bool, "flag");
    ImGui::SameLine();
    add("+ Trigger", AnimationParameterType::Trigger, "trigger");
    ImGui::SameLine();
    add("+ Vector", AnimationParameterType::Vector, "target");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("A world position (an IK target), set from scripts with SetVector");
    if (!live)
        ImGui::TextDisabled("Select an entity whose Animator uses this graph to set them live.");
}

void AnimationGraphWindow::DrawClips()
{
    AnimationGraph& graph = G();
    ImGui::TextDisabled("Clip nodes find clips by name: the Animator's own first, then these.");
    int removed = -1;
    for (int i = 0; i < static_cast<int>(graph.clips.size()); ++i)
    {
        ImGui::PushID(i);
        std::string name = graph.clips[i].name;
        ImGui::SetNextItemWidth(-ImGui::GetFrameHeight() * 1.5f);
        if (InputString("##name", name))
        {
            Begin("Rename clip");
            graph.clips[i].name = name;
            Changed();
        }
        if (ImGui::IsItemHovered())
            if (const AssetMetadata* meta = graph.clips[i].clip.GetMetadata())
                ImGui::SetTooltip("%s", meta->name.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("x"))
            removed = i;
        // Its events, edited on a copy so the step starts from what was there.
        if (ImGui::TreeNodeEx("##events", ImGuiTreeNodeFlags_SpanAvailWidth, "Events (%zu)", graph.clips[i].events.size()))
        {
            std::vector<AnimationEvent> events = graph.clips[i].events;
            const AnimationClip* clip = graph.clips[i].clip.Get().get();
            if (InspectorComponents::DrawClipEvents(events, clip ? clip->GetDuration() : 0.0f, 0.0f))
            {
                Begin("Edit events");
                G().clips[i].events = events;
                Changed();
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    if (removed >= 0)
        Apply("Remove clip", [&](AnimationGraph& g) { g.clips.erase(g.clips.begin() + removed); });

    ImGui::Button("Drop a .glb/.gltf here to add its animations", ImVec2(-FLT_MIN, 0.0f));
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MARIANA_ASSET_GLTF"))
        {
            std::vector<NamedClip> clips = GLTF::GLTFLoader::LoadClips(m_Assets, static_cast<const char*>(payload->Data));
            Apply("Add clips", [&](AnimationGraph& g) {
                for (NamedClip& clip : clips)
                {
                    const bool present = std::any_of(g.clips.begin(), g.clips.end(), [&](const NamedClip& c) { return c.clip.GetID() == clip.clip.GetID(); });
                    if (present || !clip.clip.Get())
                        continue;
                    std::string name = clip.name;
                    for (int n = 2; std::any_of(g.clips.begin(), g.clips.end(), [&](const NamedClip& c) { return c.name == name; }); ++n)
                        name = clip.name + " " + std::to_string(n);
                    g.clips.push_back({ name, clip.clip });
                }
            });
        }
        ImGui::EndDragDropTarget();
    }
}

void AnimationGraphWindow::DrawSelection()
{
    if (m_View >= 0)
    {
        if (m_SelectedTransition >= 0)
            DrawTransitionProperties(m_View, m_SelectedTransition);
        else if (m_SelectedState >= 0)
            DrawStateProperties(m_View, m_SelectedState);
        else if (m_SelectedAnyState)
            ImGui::TextWrapped("Any State: its transitions can start from whichever state is current (but not into the current one). Right-click it to make one.");
        else
            DrawNodeProperties(m_View);
        return;
    }
    if (m_SelectedNode >= 0)
        DrawNodeProperties(m_SelectedNode);
    else
        ImGui::TextDisabled("Select a node to edit it.");
}

void AnimationGraphWindow::DrawNodeProperties(int index)
{
    AnimationGraph& graph = G();
    AnimationNode& node = graph.nodes[index];
    ImGui::PushID(index);

    std::string name = node.name;
    if (InputString("Name", name))
    {
        Begin("Rename node");
        node.name = name;
        Changed();
    }
    ImGui::TextDisabled("%s node", TypeName(node.type));
    ImGui::SameLine();
    if (graph.output == index)
        ImGui::TextColored(ImVec4(0.9f, 0.75f, 0.3f, 1.0f), "(the graph's output)");
    else if (ImGui::SmallButton("Make it the output"))
        Apply("Set output", [&](AnimationGraph& g) { g.output = index; });

    auto weightEditor = [&](const char* what) {
        int parameter = node.weight.parameter;
        if (ParameterCombo(Format("%s from", what).c_str(), parameter, true, true))
            Apply("Set weight parameter", [&](AnimationGraph& g) { g.nodes[index].weight.parameter = parameter; });
        if (node.weight.parameter < 0)
        {
            float value = node.weight.value;
            if (ImGui::SliderFloat(what, &value, 0.0f, 1.0f))
            {
                Begin("Edit weight");
                node.weight.value = value;
                Changed();
            }
        }
    };
    auto connectedName = [&](int input) {
        return input >= 0 && input < static_cast<int>(graph.nodes.size()) ? graph.nodes[input].name : std::string("(not connected)");
    };

    switch (node.type)
    {
    case AnimationNodeType::Clip:
    {
        // The names it can find: the live Animator's clips (its own and the graph's), else the graph's.
        std::vector<std::string> names;
        if (const Animator* live = PreviewAnimator())
            for (int i = 0; i < live->ClipCount(); ++i)
                names.push_back(live->GetClip(i)->name);
        else
            for (const NamedClip& clip : graph.clips)
                names.push_back(clip.name);
        if (ImGui::BeginCombo("Clip", node.clip.empty() ? "(none)" : node.clip.c_str()))
        {
            for (size_t i = 0; i < names.size(); ++i)
            {
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::Selectable(names[i].c_str(), names[i] == node.clip))
                    Apply("Set clip", [&](AnimationGraph& g) { g.nodes[index].clip = names[i]; });
                ImGui::PopID();
            }
            if (names.empty())
                ImGui::TextDisabled("Select an entity using this graph to list its clips");
            ImGui::EndCombo();
        }
        std::string clip = node.clip;
        if (InputString("Clip name", clip))
        {
            Begin("Set clip");
            node.clip = clip;
            Changed();
        }
        float speed = node.speed;
        if (ImGui::DragFloat("Speed", &speed, 0.01f, -10.0f, 10.0f, "%.2f"))
        {
            Begin("Edit speed");
            node.speed = speed;
            Changed();
        }
        bool loop = node.loop;
        if (ImGui::Checkbox("Loop", &loop))
            Apply("Toggle loop", [&](AnimationGraph& g) { g.nodes[index].loop = loop; });
        std::string group = node.syncGroup;
        if (InputString("Sync group", group))
        {
            Begin("Edit sync group");
            node.syncGroup = group;
            Changed();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Clips with the same group play in step (by normalized time)");
        break;
    }
    case AnimationNodeType::Blend:
        weightEditor("Weight");
        for (size_t k = 0; k < node.inputs.size(); ++k)
            ImGui::TextDisabled("%s: %s", InputLabel(node, k).c_str(), connectedName(node.inputs[k]).c_str());
        break;
    case AnimationNodeType::TwoBoneIK:
    case AnimationNodeType::LookAt:
    {
        const bool limb = node.type == AnimationNodeType::TwoBoneIK;
        // The bone: from the previewed model's skeleton, or typed.
        const Animator* live = PreviewAnimator();
        const Skeleton* skeleton = live ? live->skeleton.Get().get() : nullptr;
        if (skeleton && ImGui::BeginCombo("Bone", node.bone.empty() ? "(none)" : node.bone.c_str()))
        {
            for (size_t b = 0; b < skeleton->BoneCount(); ++b)
            {
                ImGui::PushID(static_cast<int>(b));
                const std::string bone(Skeleton::BaseName(skeleton->GetBone(b).name));
                if (ImGui::Selectable(bone.c_str(), bone == node.bone))
                    Apply("Set IK bone", [&](AnimationGraph& g) { g.nodes[index].bone = bone; });
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        std::string bone = node.bone;
        if (InputString(skeleton ? "Bone name" : "Bone", bone))
        {
            Begin("Set IK bone");
            node.bone = bone;
            Changed();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(limb ? "The end of the limb (a hand, a foot): its parent and grandparent bend" : "The bone that looks (a head)");
        auto vectorCombo = [&](const char* label, int& parameter, bool allowNone) {
            bool changed = false;
            if (ImGui::BeginCombo(label, ParameterName(graph, parameter).c_str()))
            {
                if (allowNone && ImGui::Selectable("(none)", parameter < 0))
                {
                    parameter = -1;
                    changed = true;
                }
                for (int i = 0; i < static_cast<int>(graph.parameters.size()); ++i)
                {
                    if (graph.parameters[i].type != AnimationParameterType::Vector)
                        continue;
                    ImGui::PushID(i);
                    if (ImGui::Selectable(graph.parameters[i].name.c_str(), parameter == i))
                    {
                        parameter = i;
                        changed = true;
                    }
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
            return changed;
        };
        int target = node.parameter;
        if (vectorCombo("Target", target, false))
            Apply("Set IK target", [&](AnimationGraph& g) { g.nodes[index].parameter = target; });
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("A Vector parameter: a world position (animator:SetVector from a script)");
        if (limb)
        {
            int pole = node.parameterY;
            if (vectorCombo("Pole", pole, true))
                Apply("Set IK pole", [&](AnimationGraph& g) { g.nodes[index].parameterY = pole; });
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Where the elbow or knee points (none: as the pose bends it)");
        }
        else
        {
            int chain = node.chainLength;
            if (ImGui::SliderInt("Chain", &chain, 1, 6))
            {
                Begin("Edit look chain");
                node.chainLength = chain;
                Changed();
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Bones up from it sharing the turn (head, neck, spine...)");
            float limit = node.angleLimit;
            if (ImGui::SliderFloat("Angle limit", &limit, 0.0f, 180.0f, "%.0f deg"))
            {
                Begin("Edit look limit");
                node.angleLimit = limit;
                Changed();
            }
        }
        weightEditor("Weight");
        ImGui::TextDisabled("Pose: %s", connectedName(node.inputs.empty() ? -1 : node.inputs[0]).c_str());
        break;
    }
    case AnimationNodeType::Additive:
    {
        weightEditor("Weight");
        int reference = static_cast<int>(node.reference);
        const char* references[] = { "its clip's first frame", "the rest pose" };
        if (ImGui::Combo("Relative to", &reference, references, 2))
            Apply("Set additive reference", [&](AnimationGraph& g) { g.nodes[index].reference = static_cast<AdditiveReference>(reference); });
        break;
    }
    case AnimationNodeType::Layer:
    {
        weightEditor("Weight");
        int mode = static_cast<int>(node.layerMode);
        const char* modes[] = { "Override", "Additive" };
        if (ImGui::Combo("Mode", &mode, modes, 2))
            Apply("Set layer mode", [&](AnimationGraph& g) { g.nodes[index].layerMode = static_cast<LayerMode>(mode); });
        ImGui::SeparatorText("Bone mask");
        ImGui::TextDisabled("A bone and everything below it; none = every bone");
        int removed = -1;
        for (int i = 0; i < static_cast<int>(node.mask.entries.size()); ++i)
        {
            ImGui::PushID(i);
            BoneMask::Entry& entry = node.mask.entries[i];
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
            std::string bone = entry.bone;
            if (InputString("##bone", bone))
            {
                Begin("Edit mask");
                entry.bone = bone;
                Changed();
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() * 1.5f);
            float weight = entry.weight;
            if (ImGui::SliderFloat("##weight", &weight, 0.0f, 1.0f, "%.2f"))
            {
                Begin("Edit mask");
                entry.weight = weight;
                Changed();
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("x"))
                removed = i;
            ImGui::PopID();
        }
        if (removed >= 0)
            Apply("Edit mask", [&](AnimationGraph& g) { g.nodes[index].mask.entries.erase(g.nodes[index].mask.entries.begin() + removed); });

        // Bones to pick from: the live Animator's skeleton.
        const Animator* live = PreviewAnimator();
        const Skeleton* skeleton = live ? live->skeleton.Get().get() : nullptr;
        if (skeleton && ImGui::BeginCombo("##addbone", "+ Bone", ImGuiComboFlags_HeightLarge))
        {
            for (size_t b = 0; b < skeleton->BoneCount(); ++b)
            {
                const Skeleton::Bone& bone = skeleton->GetBone(b);
                int depth = 0;
                for (int p = bone.parent; p >= 0; p = skeleton->GetBone(p).parent) ++depth;
                const std::string label = std::string(static_cast<size_t>(std::min(depth, 12)) * 2, ' ') + bone.name;
                ImGui::PushID(static_cast<int>(b));
                if (ImGui::Selectable(label.c_str()))
                    Apply("Edit mask", [&](AnimationGraph& g) { g.nodes[index].mask.entries.push_back({ std::string(Skeleton::BaseName(bone.name)), 1.0f }); });
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        else if (!skeleton && ImGui::SmallButton("+ Bone"))
            Apply("Edit mask", [&](AnimationGraph& g) { g.nodes[index].mask.entries.push_back({ "Spine", 1.0f }); });
        break;
    }
    case AnimationNodeType::Blend1D:
    case AnimationNodeType::Blend2D:
    {
        const bool twoD = node.type == AnimationNodeType::Blend2D;
        int x = node.parameter;
        if (ParameterCombo(twoD ? "Across" : "Parameter", x, true, false))
            Apply("Set blend parameter", [&](AnimationGraph& g) { g.nodes[index].parameter = x; });
        if (twoD)
        {
            int y = node.parameterY;
            if (ParameterCombo("Up", y, true, false))
                Apply("Set blend parameter", [&](AnimationGraph& g) { g.nodes[index].parameterY = y; });
        }
        ImGui::SeparatorText("Inputs");
        int removed = -1;
        for (int i = 0; i < static_cast<int>(node.inputs.size()); ++i)
        {
            ImGui::PushID(i);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.45f);
            if (twoD && i < static_cast<int>(node.positions.size()))
            {
                float p[2] = { node.positions[i].x, node.positions[i].y };
                if (ImGui::DragFloat2("##at", p, 0.01f))
                {
                    Begin("Move blend input");
                    node.positions[i] = glm::vec2(p[0], p[1]);
                    Changed();
                }
            }
            else if (!twoD && i < static_cast<int>(node.thresholds.size()))
            {
                float t = node.thresholds[i];
                if (ImGui::DragFloat("##at", &t, 0.01f))
                {
                    Begin("Move blend input");
                    node.thresholds[i] = t;
                    Changed();
                }
            }
            ImGui::SameLine();
            ImGui::TextUnformatted(connectedName(node.inputs[i]).c_str());
            ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - ImGui::GetFrameHeight());
            if (ImGui::SmallButton("x"))
                removed = i;
            ImGui::PopID();
        }
        if (removed >= 0)
            Apply("Remove blend input", [&](AnimationGraph& g) {
                AnimationNode& n = g.nodes[index];
                n.inputs.erase(n.inputs.begin() + removed);
                if (removed < static_cast<int>(n.thresholds.size())) n.thresholds.erase(n.thresholds.begin() + removed);
                if (removed < static_cast<int>(n.positions.size())) n.positions.erase(n.positions.begin() + removed);
            });
        if (ImGui::SmallButton("+ Input"))
            Apply("Add blend input", [&](AnimationGraph& g) {
                AnimationNode& n = g.nodes[index];
                n.inputs.push_back(-1);
                if (twoD) n.positions.push_back(glm::vec2(0.0f));
                else n.thresholds.push_back(n.thresholds.empty() ? 0.0f : n.thresholds.back() + 1.0f);
            });
        if (!twoD)
            ImGui::TextDisabled("Thresholds go up from first to last");
        break;
    }
    case AnimationNodeType::StateMachine:
    {
        if (ImGui::Button("Open its states"))
            m_View = index;
        if (!node.states.empty())
        {
            int entry = std::clamp(node.entryState, 0, static_cast<int>(node.states.size()) - 1);
            if (ImGui::BeginCombo("Entry state", node.states[entry].c_str()))
            {
                for (int s = 0; s < static_cast<int>(node.states.size()); ++s)
                    if (ImGui::Selectable(node.states[s].c_str(), s == entry))
                        Apply("Set entry state", [&](AnimationGraph& g) { g.nodes[index].entryState = s; });
                ImGui::EndCombo();
            }
        }
        ImGui::TextDisabled("%zu states, %zu transitions", node.states.size(), node.transitions.size());
        break;
    }
    }

    ImGui::Spacing();
    if (ImGui::Button("Delete node"))
    {
        Apply("Delete node", [&](AnimationGraph& g) { g.RemoveNode(index); });
        m_SelectedNode = -1;
        if (m_View == index)
            m_View = -1;
    }
    ImGui::PopID();
}

void AnimationGraphWindow::DrawStateProperties(int machineIndex, int state)
{
    AnimationGraph& graph = G();
    AnimationNode& machine = graph.nodes[machineIndex];
    ImGui::PushID(state);

    std::string name = machine.states[state];
    if (InputString("State", name))
    {
        Begin("Rename state");
        machine.states[state] = name;
        Changed();
    }
    const int motion = state < static_cast<int>(machine.inputs.size()) ? machine.inputs[state] : -1;
    const char* motionName = motion >= 0 && motion < static_cast<int>(graph.nodes.size()) ? graph.nodes[motion].name.c_str() : "(none)";
    if (ImGui::BeginCombo("Plays", motionName))
    {
        for (int n = 0; n < static_cast<int>(graph.nodes.size()); ++n)
        {
            if (n == machineIndex || Feeds(graph, machineIndex, n))
                continue;
            ImGui::PushID(n);
            if (ImGui::Selectable(graph.nodes[n].name.c_str(), n == motion))
                Apply("Set state motion", [&](AnimationGraph& g) { g.nodes[machineIndex].inputs[state] = n; });
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    if (motion >= 0 && ImGui::SmallButton("Show its node"))
    {
        m_View = -1;
        m_SelectedNode = motion;
        m_GraphCanvas.CenterOn(graph.nodes[motion].position + glm::vec2(kNodeWidth * 0.5f, 40.0f));
    }

    bool entry = machine.entryState == state;
    if (ImGui::Checkbox("Entry state", &entry) && entry)
        Apply("Set entry state", [&](AnimationGraph& g) { g.nodes[machineIndex].entryState = state; });

    ImGui::SeparatorText("Transitions out");
    for (int t = 0; t < static_cast<int>(machine.transitions.size()); ++t)
    {
        const AnimationTransition& transition = machine.transitions[t];
        if (transition.from != state)
            continue;
        ImGui::PushID(t);
        if (ImGui::Selectable(("-> " + machine.states[transition.to]).c_str()))
        {
            m_SelectedTransition = t;
            m_SelectedState = -1;
        }
        ImGui::PopID();
    }
    ImGui::TextDisabled("Right-click the state on the canvas to make one");

    ImGui::Spacing();
    if (ImGui::Button("Delete state"))
    {
        Apply("Delete state", [&](AnimationGraph& g) { g.nodes[machineIndex].RemoveState(state); });
        m_SelectedState = -1;
    }
    ImGui::PopID();
}

void AnimationGraphWindow::DrawTransitionProperties(int machineIndex, int index)
{
    AnimationGraph& graph = G();
    AnimationNode& machine = graph.nodes[machineIndex];
    AnimationTransition& transition = machine.transitions[index];
    ImGui::PushID(index);

    auto stateName = [&](int s) { return s >= 0 && s < static_cast<int>(machine.states.size()) ? machine.states[s] : std::string("Any State"); };
    ImGui::Text("%s  ->  %s", stateName(transition.from).c_str(), stateName(transition.to).c_str());

    float duration = transition.duration;
    if (ImGui::DragFloat("Fade (s)", &duration, 0.01f, 0.0f, 10.0f, "%.2f"))
    {
        Begin("Edit transition");
        transition.duration = std::max(duration, 0.0f);
        Changed();
    }
    bool hasExit = transition.exitTime >= 0.0f;
    if (ImGui::Checkbox("Exit time", &hasExit))
        Apply("Edit transition", [&](AnimationGraph& g) { g.nodes[machineIndex].transitions[index].exitTime = hasExit ? 1.0f : -1.0f; });
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Not before the state it leaves has played this much (1 = one loop of its clips)");
    if (hasExit)
    {
        ImGui::SameLine();
        float exit = transition.exitTime;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::DragFloat("##exit", &exit, 0.01f, 0.0f, 100.0f, "%.2f"))
        {
            Begin("Edit transition");
            transition.exitTime = std::max(exit, 0.0f);
            Changed();
        }
    }
    bool interruptible = transition.interruptible;
    if (ImGui::Checkbox("Can be interrupted", &interruptible))
        Apply("Edit transition", [&](AnimationGraph& g) { g.nodes[machineIndex].transitions[index].interruptible = interruptible; });

    ImGui::SeparatorText("When (all of these)");
    int removed = -1;
    for (int c = 0; c < static_cast<int>(transition.conditions.size()); ++c)
    {
        AnimationCondition& condition = transition.conditions[c];
        ImGui::PushID(c);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.4f);
        int parameter = condition.parameter;
        if (ParameterCombo("##parameter", parameter, false, false))
            Apply("Edit condition", [&](AnimationGraph& g) {
                AnimationCondition& cond = g.nodes[machineIndex].transitions[index].conditions[c];
                cond.parameter = parameter;
                const bool isFloat = g.parameters[parameter].type == AnimationParameterType::Float;
                if (isFloat != (cond.mode != ConditionMode::If && cond.mode != ConditionMode::IfNot))
                    cond.mode = isFloat ? ConditionMode::Greater : ConditionMode::If;
            });
        const bool isFloat = condition.parameter >= 0 && condition.parameter < static_cast<int>(graph.parameters.size()) &&
                             graph.parameters[condition.parameter].type == AnimationParameterType::Float;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(isFloat ? ImGui::GetFontSize() * 3.5f : ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() * 1.5f);
        if (ImGui::BeginCombo("##mode", kModeNames[static_cast<int>(condition.mode)]))
        {
            for (int m = 0; m < 6; ++m)
            {
                const bool floatMode = m >= static_cast<int>(ConditionMode::Greater);
                if (floatMode != isFloat)
                    continue;
                if (ImGui::Selectable(kModeNames[m], static_cast<int>(condition.mode) == m))
                    Apply("Edit condition", [&](AnimationGraph& g) { g.nodes[machineIndex].transitions[index].conditions[c].mode = static_cast<ConditionMode>(m); });
            }
            ImGui::EndCombo();
        }
        if (isFloat)
        {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() * 1.5f);
            float threshold = condition.threshold;
            if (ImGui::DragFloat("##threshold", &threshold, 0.01f))
            {
                Begin("Edit condition");
                condition.threshold = threshold;
                Changed();
            }
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("x"))
            removed = c;
        ImGui::PopID();
    }
    if (removed >= 0)
        Apply("Remove condition", [&](AnimationGraph& g) { auto& list = g.nodes[machineIndex].transitions[index].conditions; list.erase(list.begin() + removed); });
    if (ImGui::SmallButton("+ Condition"))
    {
        if (graph.parameters.empty())
            m_Status = "Add a parameter first";
        else
            Apply("Add condition", [&](AnimationGraph& g) {
                const bool isFloat = g.parameters[0].type == AnimationParameterType::Float;
                g.nodes[machineIndex].transitions[index].conditions.push_back({ 0, isFloat ? ConditionMode::Greater : ConditionMode::If, 0.0f });
            });
    }
    if (transition.conditions.empty() && transition.exitTime < 0.0f)
        ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.3f, 1.0f), "Needs a condition or an exit time");

    ImGui::SeparatorText("Order");
    ImGui::TextDisabled("The first transition that holds wins");
    ImGui::BeginDisabled(index == 0);
    if (ImGui::SmallButton("Earlier"))
    {
        Apply("Reorder transitions", [&](AnimationGraph& g) { std::swap(g.nodes[machineIndex].transitions[index], g.nodes[machineIndex].transitions[index - 1]); });
        --m_SelectedTransition;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(index + 1 >= static_cast<int>(machine.transitions.size()));
    if (ImGui::SmallButton("Later"))
    {
        Apply("Reorder transitions", [&](AnimationGraph& g) { std::swap(g.nodes[machineIndex].transitions[index], g.nodes[machineIndex].transitions[index + 1]); });
        ++m_SelectedTransition;
    }
    ImGui::EndDisabled();

    ImGui::Spacing();
    if (ImGui::Button("Delete transition"))
    {
        Apply("Delete transition", [&](AnimationGraph& g) { auto& list = g.nodes[machineIndex].transitions; list.erase(list.begin() + index); });
        m_SelectedTransition = -1;
    }
    ImGui::PopID();
}

AnimationGraphWindow::NodeLayout AnimationGraphWindow::Layout(int index) const
{
    const AnimationNode& node = m_Graph.Get()->nodes[index];
    NodeLayout layout;
    const size_t rows = std::max<size_t>(node.inputs.size(), 1);
    layout.min = node.position;
    layout.max = node.position + glm::vec2(kNodeWidth, kHeader + kRow * static_cast<float>(rows) + 8.0f);
    layout.output = glm::vec2(layout.max.x, node.position.y + kHeader * 0.5f);
    for (size_t k = 0; k < node.inputs.size(); ++k)
        layout.inputs.push_back(glm::vec2(node.position.x, node.position.y + kHeader + kRow * (static_cast<float>(k) + 0.5f) + 4.0f));
    return layout;
}

std::string AnimationGraphWindow::UniqueNodeName(const std::string& base) const
{
    std::string name = base;
    for (int n = 2; m_Graph.Get()->FindNode(name) >= 0; ++n)
        name = base + " " + std::to_string(n);
    return name;
}

int AnimationGraphWindow::NewNode(AnimationNodeType type, glm::vec2 at)
{
    AnimationNode node;
    node.type = type;
    node.name = UniqueNodeName(TypeName(type));
    node.position = glm::round(at / 8.0f) * 8.0f;
    switch (type)
    {
    case AnimationNodeType::Clip:
        if (const Animator* live = PreviewAnimator(); live && live->ClipCount() > 0)
            node.clip = live->GetClip(0)->name;
        else if (!G().clips.empty())
            node.clip = G().clips[0].name;
        break;
    case AnimationNodeType::Blend:
        node.inputs = { -1, -1 };
        node.weight = { 0.5f, -1 };
        break;
    case AnimationNodeType::Blend1D:
        node.inputs = { -1, -1 };
        node.thresholds = { 0.0f, 1.0f };
        break;
    case AnimationNodeType::Blend2D:
        node.inputs = { -1, -1, -1, -1 };
        node.positions = { { 0.0f, 1.0f }, { -1.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, -1.0f } };
        break;
    case AnimationNodeType::Additive:
    case AnimationNodeType::Layer:
        node.inputs = { -1, -1 };
        break;
    case AnimationNodeType::StateMachine:
        break;
    case AnimationNodeType::TwoBoneIK:
    case AnimationNodeType::LookAt:
        node.inputs = { -1 };
        node.bone = type == AnimationNodeType::LookAt ? "Head" : "RightHand";
        for (int i = 0; i < static_cast<int>(G().parameters.size()) && node.parameter < 0; ++i)
            if (G().parameters[i].type == AnimationParameterType::Vector)
                node.parameter = i;
        break;
    }
    return G().AddNode(std::move(node));
}

void AnimationGraphWindow::AddNodeMenu(glm::vec2 at)
{
    for (int t = 0; t < kTypeCount; ++t)
    {
        if (t == static_cast<int>(AnimationNodeType::TwoBoneIK))
            ImGui::Separator();
        if (ImGui::MenuItem(kTypeNames[t]))
        {
            int added = -1;
            Apply("Add node", [&](AnimationGraph&) { added = NewNode(static_cast<AnimationNodeType>(t), at); });
            m_SelectedNode = added;
        }
    }
}

void AnimationGraphWindow::ArrangeGraph()
{
    AnimationGraph& graph = G();
    const int count = static_cast<int>(graph.nodes.size());
    // Columns by distance from the output (the output on the right), unreached nodes further left.
    std::vector<int> depth(count, -1);
    std::function<void(int, int)> visit = [&](int n, int d) {
        if (n < 0 || n >= count || depth[n] >= d || d > count)
            return;
        depth[n] = d;
        for (int input : graph.nodes[n].inputs)
            visit(input, d + 1);
    };
    visit(graph.output, 0);
    int deepest = 0;
    for (int d : depth) deepest = std::max(deepest, d);
    for (int& d : depth) if (d < 0) d = deepest + 1;

    std::vector<float> columnHeight(count + 2, 0.0f);
    for (int n = 0; n < count; ++n)
    {
        const NodeLayout layout = Layout(n);
        graph.nodes[n].position = glm::vec2(-static_cast<float>(depth[n]) * 270.0f, columnHeight[depth[n]]);
        columnHeight[depth[n]] += (layout.max.y - layout.min.y) + 30.0f;
    }
    // Columns centered on each other.
    for (int n = 0; n < count; ++n)
        graph.nodes[n].position.y -= columnHeight[depth[n]] * 0.5f;
    m_FrameGraph = true;
    m_LayoutDirty = true;
}

void AnimationGraphWindow::Frame(GraphCanvas& canvas, glm::vec2 min, glm::vec2 max)
{
    const glm::vec2 margin(60.0f);
    min -= margin;
    max += margin;
    const glm::vec2 extent = glm::max(max - min, glm::vec2(1.0f));
    canvas.zoom = std::clamp(std::min(canvas.size.x / extent.x, canvas.size.y / extent.y), 0.4f, 1.0f);
    canvas.CenterOn((min + max) * 0.5f);
}

void AnimationGraphWindow::ArrangeMachine(int machineIndex)
{
    AnimationNode& machine = G().nodes[machineIndex];
    machine.statePositions.resize(machine.states.size());
    for (size_t s = 0; s < machine.states.size(); ++s)
        machine.statePositions[s] = glm::vec2(static_cast<float>(s % 3) * 230.0f, static_cast<float>(s / 3) * 120.0f);
    machine.anyStatePosition = glm::vec2(-250.0f, 0.0f);
    m_LayoutDirty = true;
}

void AnimationGraphWindow::DrawGraphView()
{
    AnimationGraph& graph = G();
    GraphCanvas& canvas = m_GraphCanvas;
    canvas.Begin("##graphcanvas", ImGui::GetContentRegionAvail());
    const bool pressed = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    const bool doubleClicked = canvas.hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 mouse = ImGui::GetMousePos();
    const int count = static_cast<int>(graph.nodes.size());
    Animator* live = PreviewAnimator();
    auto liveWeight = [&](int n) {
        return live && n < static_cast<int>(live->Nodes().size()) ? live->Nodes()[n].weight : -1.0f;
    };

    std::vector<NodeLayout> layouts;
    layouts.reserve(count);
    for (int n = 0; n < count; ++n)
        layouts.push_back(Layout(n));
    if (m_FrameGraph && count > 0)
    {
        glm::vec2 min(1e9f), max(-1e9f);
        for (const NodeLayout& layout : layouts)
        {
            min = glm::min(min, layout.min);
            max = glm::max(max, layout.max);
        }
        Frame(canvas, min, max);
    }
    m_FrameGraph = false;

    auto nearPin = [&](glm::vec2 p) {
        const ImVec2 s = canvas.ToScreen(p);
        return (s.x - mouse.x) * (s.x - mouse.x) + (s.y - mouse.y) * (s.y - mouse.y) < kPinHit * kPinHit;
    };
    auto nodeAt = [&](ImVec2 screen) {
        const glm::vec2 p = canvas.ToGraph(screen);
        for (int n = count - 1; n >= 0; --n)
            if (p.x >= layouts[n].min.x && p.x <= layouts[n].max.x && p.y >= layouts[n].min.y && p.y <= layouts[n].max.y)
                return n;
        return -1;
    };
    auto inputAt = [&](int& node, int& slot) {
        for (int n = 0; n < count; ++n)
            for (int k = 0; k < static_cast<int>(layouts[n].inputs.size()); ++k)
                if (nearPin(layouts[n].inputs[k]))
                {
                    node = n;
                    slot = k;
                    return true;
                }
        return false;
    };
    auto outputAt = [&]() {
        for (int n = 0; n < count; ++n)
            if (nearPin(layouts[n].output))
                return n;
        return -1;
    };
    auto connect = [&](int source, int target, int slot) {
        if (Feeds(graph, target, source))
        {
            m_Status = "That would make a loop";
            return;
        }
        Apply("Connect", [&](AnimationGraph& g) { g.nodes[target].inputs[slot] = source; });
    };

    // Clicks: pins first, then node bodies.
    if (pressed)
    {
        int node = -1, slot = -1;
        if (const int out = outputAt(); out >= 0)
        {
            m_Drag = DragKind::Link;
            m_DragIndex = out;
            m_LinkInput = -1;
            m_LinkFromInput = false;
        }
        else if (inputAt(node, slot))
        {
            const int source = graph.nodes[node].inputs[slot];
            if (source >= 0 && source < count)
            {
                // Pick the link up off the input.
                Begin("Connect");
                graph.nodes[node].inputs[slot] = -1;
                Changed();
                m_Drag = DragKind::Link;
                m_DragIndex = source;
                m_LinkFromInput = false;
            }
            else
            {
                m_Drag = DragKind::Link;
                m_DragIndex = node;
                m_LinkInput = slot;
                m_LinkFromInput = true;
            }
        }
        else if (const int hit = nodeAt(mouse); hit >= 0)
        {
            m_SelectedNode = hit;
            m_Drag = DragKind::Node;
            m_DragIndex = hit;
            m_DragOffset = canvas.MouseInGraph() - graph.nodes[hit].position;
        }
        else
        {
            m_SelectedNode = -1;
        }
    }
    if (doubleClicked)
    {
        if (const int hit = nodeAt(mouse); hit >= 0 && graph.nodes[hit].type == AnimationNodeType::StateMachine)
        {
            m_View = hit;
            m_Drag = DragKind::None;
        }
    }

    if (m_Drag == DragKind::Node && m_DragIndex >= 0 && m_DragIndex < count)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            const glm::vec2 target = glm::round((canvas.MouseInGraph() - m_DragOffset) / 8.0f) * 8.0f;
            if (target != graph.nodes[m_DragIndex].position)
            {
                Begin("Move node");
                graph.nodes[m_DragIndex].position = target;
                m_LayoutDirty = true;
                layouts[m_DragIndex] = Layout(m_DragIndex);
            }
        }
        else
            m_Drag = DragKind::None;
    }
    if (m_Drag == DragKind::Link && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        int node = -1, slot = -1;
        if (!m_LinkFromInput && inputAt(node, slot) && node != m_DragIndex)
        {
            m_Drag = DragKind::None;
            connect(m_DragIndex, node, slot);
        }
        else if (m_LinkFromInput)
        {
            const int out = outputAt();
            m_Drag = DragKind::None;
            if (out >= 0 && out != m_DragIndex)
                connect(out, m_DragIndex, m_LinkInput);
        }
        m_Drag = DragKind::None;
    }

    // Links under the nodes; brighter and thicker the more of the pose goes through them.
    for (int n = 0; n < count; ++n)
    {
        for (int k = 0; k < static_cast<int>(graph.nodes[n].inputs.size()); ++k)
        {
            const int source = graph.nodes[n].inputs[k];
            if (source < 0 || source >= count)
                continue;
            // What flows through: no more than the input and the node it feeds make.
            const float w = std::min(liveWeight(source), liveWeight(n));
            const ImU32 color = w > 0.0f ? IM_COL32(255, static_cast<int>(150 + 80 * w), 80, static_cast<int>(120 + 135 * w)) : IM_COL32(170, 170, 180, 200);
            GraphDraw::Link(dl, canvas.ToScreen(layouts[source].output), canvas.ToScreen(layouts[n].inputs[k]), color, (1.5f + (w > 0.0f ? 3.0f * w : 0.0f)) * canvas.zoom);
        }
    }

    // Nodes.
    for (int n = 0; n < count; ++n)
    {
        const AnimationNode& node = graph.nodes[n];
        const NodeLayout& layout = layouts[n];
        const ImVec2 min = canvas.ToScreen(layout.min), max = canvas.ToScreen(layout.max);
        const float rounding = 6.0f * canvas.zoom;
        dl->AddRectFilled(min, max, IM_COL32(36, 38, 44, 240), rounding);
        dl->AddRectFilled(min, canvas.ToScreen(glm::vec2(layout.max.x, layout.min.y + kHeader)), TypeColor(node.type), rounding, ImDrawFlags_RoundCornersTop);

        dl->PushClipRect(min, max, true);
        GraphDraw::Text(dl, canvas, layout.min + glm::vec2(8.0f, 4.0f), IM_COL32(255, 255, 255, 255), node.name.c_str());
        GraphDraw::Text(dl, canvas, layout.min + glm::vec2(8.0f, 22.0f), IM_COL32(230, 230, 235, 190), Subtitle(graph, node).c_str(), 0.85f);
        for (size_t k = 0; k < layout.inputs.size(); ++k)
            GraphDraw::Text(dl, canvas, layout.inputs[k] + glm::vec2(12.0f, -8.0f), IM_COL32(210, 210, 220, 220), InputLabel(node, k).c_str(), 0.85f);
        if (const float w = liveWeight(n); w >= 0.0f)
        {
            // How much of the pose it makes, along the bottom.
            const ImVec2 barMin = canvas.ToScreen(glm::vec2(layout.min.x + 6.0f, layout.max.y - 6.0f));
            const ImVec2 barMax = canvas.ToScreen(glm::vec2(layout.min.x + 6.0f + (kNodeWidth - 12.0f) * std::min(w, 1.0f), layout.max.y - 3.0f));
            dl->AddRectFilled(barMin, barMax, IM_COL32(240, 190, 70, 255));
            if (node.type == AnimationNodeType::StateMachine && n < static_cast<int>(live->Machines().size()) && !live->Machines()[n].active.empty())
            {
                const int state = live->Machines()[n].active.back().state;
                if (state < static_cast<int>(node.states.size()))
                    GraphDraw::Text(dl, canvas, glm::vec2(layout.max.x - 8.0f - GraphDraw::TextWidth(node.states[state].c_str(), 0.85f), layout.min.y + 4.0f),
                                    IM_COL32(255, 230, 160, 255), node.states[state].c_str(), 0.85f);
            }
        }
        dl->PopClipRect();

        const bool selected = n == m_SelectedNode;
        const bool output = n == graph.output;
        dl->AddRect(min, max, selected ? IM_COL32(255, 205, 90, 255) : output ? IM_COL32(220, 175, 70, 220) : IM_COL32(85, 88, 98, 255), rounding, 0,
                    (selected || output ? 2.5f : 1.0f) * canvas.zoom);
        if (output)
            GraphDraw::Text(dl, canvas, glm::vec2(layout.min.x, layout.min.y - 18.0f), IM_COL32(230, 185, 80, 255), "OUTPUT", 0.8f);

        for (size_t k = 0; k < layout.inputs.size(); ++k)
        {
            const bool connected = node.inputs[k] >= 0;
            dl->AddCircleFilled(canvas.ToScreen(layout.inputs[k]), kPinRadius * canvas.zoom, connected ? IM_COL32(200, 200, 210, 255) : IM_COL32(60, 62, 70, 255));
            dl->AddCircle(canvas.ToScreen(layout.inputs[k]), kPinRadius * canvas.zoom, IM_COL32(200, 200, 210, 255));
        }
        dl->AddCircleFilled(canvas.ToScreen(layout.output), kPinRadius * canvas.zoom, IM_COL32(230, 200, 120, 255));
    }

    if (m_Drag == DragKind::Link && m_DragIndex >= 0 && m_DragIndex < count)
    {
        if (m_LinkFromInput && m_LinkInput < static_cast<int>(layouts[m_DragIndex].inputs.size()))
            GraphDraw::Link(dl, mouse, canvas.ToScreen(layouts[m_DragIndex].inputs[m_LinkInput]), IM_COL32(255, 220, 120, 255), 2.0f * canvas.zoom);
        else if (!m_LinkFromInput)
            GraphDraw::Link(dl, canvas.ToScreen(layouts[m_DragIndex].output), mouse, IM_COL32(255, 220, 120, 255), 2.0f * canvas.zoom);
    }
    if (count == 0)
        dl->AddText(ImVec2(canvas.origin.x + 16.0f, canvas.origin.y + 16.0f), ImGui::GetColorU32(ImGuiCol_TextDisabled), "Right-click to add a node");
    canvas.End();

    // Menus.
    if (canvas.RightClicked())
    {
        m_ContextPoint = canvas.MouseInGraph();
        m_ContextTarget = nodeAt(mouse);
        if (m_ContextTarget >= 0)
            m_SelectedNode = m_ContextTarget;
        ImGui::OpenPopup(m_ContextTarget >= 0 ? "##nodemenu" : "##addmenu");
    }
    if (ImGui::BeginPopup("##addmenu"))
    {
        ImGui::TextDisabled("Add");
        AddNodeMenu(m_ContextPoint);
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("##nodemenu"))
    {
        const int target = m_ContextTarget;
        if (target >= 0 && target < count)
        {
            ImGui::TextDisabled("%s", graph.nodes[target].name.c_str());
            if (graph.nodes[target].type == AnimationNodeType::StateMachine && ImGui::MenuItem("Open its states"))
                m_View = target;
            if (ImGui::MenuItem("Make it the output", nullptr, graph.output == target))
                Apply("Set output", [&](AnimationGraph& g) { g.output = target; });
            if (ImGui::MenuItem("Duplicate"))
            {
                int added = -1;
                Apply("Duplicate node", [&](AnimationGraph& g) {
                    AnimationNode copy = g.nodes[target];
                    copy.name = UniqueNodeName(copy.name);
                    copy.position += glm::vec2(24.0f, 24.0f);
                    added = g.AddNode(std::move(copy));
                });
                m_SelectedNode = added;
            }
            if (ImGui::MenuItem("Delete", "Del"))
            {
                Apply("Delete node", [&](AnimationGraph& g) { g.RemoveNode(target); });
                m_SelectedNode = -1;
            }
        }
        ImGui::EndPopup();
    }

    if (ImGui::IsWindowFocused() && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete) && m_SelectedNode >= 0 && m_SelectedNode < count)
    {
        const int target = m_SelectedNode;
        Apply("Delete node", [&](AnimationGraph& g) { g.RemoveNode(target); });
        m_SelectedNode = -1;
    }
}

void AnimationGraphWindow::DrawMachineView(int machineIndex)
{
    AnimationGraph& graph = G();
    AnimationNode& machine = graph.nodes[machineIndex];
    machine.statePositions.resize(machine.states.size(), glm::vec2(0.0f));
    const auto [found, created] = m_MachineCanvases.try_emplace(machineIndex);
    GraphCanvas& canvas = found->second;
    canvas.Begin("##machinecanvas", ImGui::GetContentRegionAvail());
    if (created)
    {
        // First look at this machine: fit its boxes.
        glm::vec2 min = machine.anyStatePosition, max = machine.anyStatePosition + kStateSize;
        for (const glm::vec2& p : machine.statePositions)
        {
            min = glm::min(min, p);
            max = glm::max(max, p + kStateSize);
        }
        Frame(canvas, min, max);
    }
    const bool pressed = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    const bool doubleClicked = canvas.hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 mouse = ImGui::GetMousePos();
    const int states = static_cast<int>(machine.states.size());

    Animator* live = PreviewAnimator();
    const std::vector<Animator::ActiveState>* active =
        live && machineIndex < static_cast<int>(live->Machines().size()) ? &live->Machines()[machineIndex].active : nullptr;

    auto boxMin = [&](int s) { return s == -1 ? machine.anyStatePosition : machine.statePositions[s]; };
    auto boxScreen = [&](int s, ImVec2& min, ImVec2& max) {
        min = canvas.ToScreen(boxMin(s));
        max = canvas.ToScreen(boxMin(s) + kStateSize);
    };
    auto stateAt = [&](ImVec2 screen) {
        const glm::vec2 p = canvas.ToGraph(screen);
        for (int s = states - 1; s >= -1; --s)
        {
            const glm::vec2 min = boxMin(s), max = min + kStateSize;
            if (p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y)
                return s;
        }
        return -2;
    };
    // Each transition's line on the screen, moved aside when the reverse one exists too.
    auto transitionLine = [&](const AnimationTransition& t, ImVec2& a, ImVec2& b) {
        ImVec2 fromMin, fromMax, toMin, toMax;
        boxScreen(t.from, fromMin, fromMax);
        boxScreen(t.to, toMin, toMax);
        const ImVec2 c1((fromMin.x + fromMax.x) * 0.5f, (fromMin.y + fromMax.y) * 0.5f), c2((toMin.x + toMax.x) * 0.5f, (toMin.y + toMax.y) * 0.5f);
        a = BoxEdge(fromMin, fromMax, c2);
        b = BoxEdge(toMin, toMax, c1);
        const bool reverse = std::any_of(machine.transitions.begin(), machine.transitions.end(), [&](const AnimationTransition& o) { return o.from == t.to && o.to == t.from; });
        if (reverse)
        {
            const float dx = b.x - a.x, dy = b.y - a.y, length = std::max(std::sqrt(dx * dx + dy * dy), 1.0f);
            const ImVec2 n(-dy / length * 7.0f * canvas.zoom, dx / length * 7.0f * canvas.zoom);
            a = ImVec2(a.x + n.x, a.y + n.y);
            b = ImVec2(b.x + n.x, b.y + n.y);
        }
    };

    if (m_Drag == DragKind::Transition)
    {
        // Making a transition: click the state it goes to.
        if (pressed)
        {
            const int target = stateAt(mouse);
            const int from = m_DragIndex;
            m_Drag = DragKind::None;
            if (target >= 0 && target != from)
            {
                Apply("Add transition", [&](AnimationGraph& g) {
                    AnimationTransition t;
                    t.from = from;
                    t.to = target;
                    t.exitTime = 1.0f;
                    g.nodes[machineIndex].transitions.push_back(t);
                });
                m_SelectedTransition = static_cast<int>(machine.transitions.size()) - 1;
                m_SelectedState = -1;
                m_SelectedAnyState = false;
            }
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_Escape) || canvas.RightClicked())
            m_Drag = DragKind::None;
    }
    else if (pressed)
    {
        const int hit = stateAt(mouse);
        m_SelectedTransition = -1;
        m_SelectedState = -1;
        m_SelectedAnyState = false;
        if (hit >= -1)
        {
            if (hit == -1) m_SelectedAnyState = true;
            else m_SelectedState = hit;
            m_Drag = DragKind::State;
            m_DragIndex = hit;
            m_DragOffset = canvas.MouseInGraph() - boxMin(hit);
        }
        else
        {
            for (int t = 0; t < static_cast<int>(machine.transitions.size()); ++t)
            {
                ImVec2 a, b;
                transitionLine(machine.transitions[t], a, b);
                if (GraphDraw::DistanceToSegment(mouse, a, b) < 6.0f)
                {
                    m_SelectedTransition = t;
                    break;
                }
            }
        }
    }
    if (doubleClicked)
    {
        // Into the node the state plays.
        const int hit = stateAt(mouse);
        if (hit >= 0 && hit < static_cast<int>(machine.inputs.size()) && machine.inputs[hit] >= 0)
        {
            m_View = -1;
            m_SelectedNode = machine.inputs[hit];
            m_GraphCanvas.CenterOn(graph.nodes[m_SelectedNode].position + glm::vec2(kNodeWidth * 0.5f, 40.0f));
            m_Drag = DragKind::None;
        }
    }
    if (m_Drag == DragKind::State)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            const glm::vec2 target = glm::round((canvas.MouseInGraph() - m_DragOffset) / 8.0f) * 8.0f;
            glm::vec2& position = m_DragIndex == -1 ? machine.anyStatePosition : machine.statePositions[m_DragIndex];
            if (target != position)
            {
                Begin("Move state");
                position = target;
                m_LayoutDirty = true;
            }
        }
        else
            m_Drag = DragKind::None;
    }

    // Transitions, then the boxes over them.
    int running = -1, runningFrom = -1;
    if (active && active->size() >= 2 && active->back().fade < 1.0f)
    {
        running = active->back().state;
        runningFrom = (*active)[active->size() - 2].state;
    }
    for (int t = 0; t < static_cast<int>(machine.transitions.size()); ++t)
    {
        const AnimationTransition& transition = machine.transitions[t];
        if (transition.to < 0 || transition.to >= states || transition.from >= states)
            continue;
        ImVec2 a, b;
        transitionLine(transition, a, b);
        const bool selected = t == m_SelectedTransition;
        const bool now = transition.to == running && (transition.from == runningFrom || transition.from == -1);
        const ImU32 color = selected ? IM_COL32(255, 205, 90, 255) : now ? IM_COL32(255, 160, 70, 255) : IM_COL32(190, 190, 200, 220);
        GraphDraw::Arrow(dl, a, b, color, (selected || now ? 2.5f : 1.5f) * canvas.zoom, 9.0f * canvas.zoom);
    }

    for (int s = -1; s < states; ++s)
    {
        ImVec2 min, max;
        boxScreen(s, min, max);
        const bool isAny = s == -1;
        const bool current = active && !active->empty() && active->back().state == s;
        const bool selected = isAny ? m_SelectedAnyState : s == m_SelectedState;
        const ImU32 fill = isAny ? IM_COL32(40, 96, 100, 245) : current ? IM_COL32(150, 92, 40, 250) : IM_COL32(52, 56, 66, 245);
        const float rounding = 8.0f * canvas.zoom;
        dl->AddRectFilled(min, max, fill, rounding);
        const bool entry = s == machine.entryState;
        dl->AddRect(min, max, selected ? IM_COL32(255, 205, 90, 255) : entry ? IM_COL32(110, 200, 110, 255) : IM_COL32(95, 100, 112, 255), rounding, 0,
                    (selected || entry ? 2.5f : 1.0f) * canvas.zoom);

        const char* title = isAny ? "Any State" : machine.states[s].c_str();
        const glm::vec2 boxPos = boxMin(s);
        dl->PushClipRect(min, max, true);
        GraphDraw::Text(dl, canvas, boxPos + glm::vec2((kStateSize.x - GraphDraw::TextWidth(title)) * 0.5f, 6.0f), IM_COL32(255, 255, 255, 255), title);
        if (!isAny)
        {
            const int motion = s < static_cast<int>(machine.inputs.size()) ? machine.inputs[s] : -1;
            const std::string plays = motion >= 0 && motion < static_cast<int>(graph.nodes.size()) ? graph.nodes[motion].name : "(plays nothing)";
            GraphDraw::Text(dl, canvas, boxPos + glm::vec2((kStateSize.x - GraphDraw::TextWidth(plays.c_str(), 0.8f)) * 0.5f, 25.0f), IM_COL32(220, 220, 230, 170), plays.c_str(), 0.8f);
        }
        if (current)
        {
            // How far it got (within a loop).
            const float t = active->back().normalizedTime - std::floor(active->back().normalizedTime);
            dl->AddRectFilled(canvas.ToScreen(boxPos + glm::vec2(6.0f, kStateSize.y - 6.0f)),
                              canvas.ToScreen(boxPos + glm::vec2(6.0f + (kStateSize.x - 12.0f) * t, kStateSize.y - 3.0f)), IM_COL32(255, 220, 140, 255));
        }
        dl->PopClipRect();
        if (entry && !isAny)
            GraphDraw::Text(dl, canvas, boxPos + glm::vec2(0.0f, -18.0f), IM_COL32(120, 210, 120, 255), "ENTRY", 0.8f);
    }

    if (m_Drag == DragKind::Transition)
    {
        ImVec2 min, max;
        boxScreen(m_DragIndex, min, max);
        GraphDraw::Arrow(dl, BoxEdge(min, max, mouse), mouse, IM_COL32(255, 220, 120, 255), 2.0f * canvas.zoom, 9.0f * canvas.zoom);
        dl->AddText(ImVec2(canvas.origin.x + 16.0f, canvas.origin.y + 16.0f), ImGui::GetColorU32(ImGuiCol_Text), "Click the state it goes to (Esc cancels)");
    }
    else if (states == 0)
        dl->AddText(ImVec2(canvas.origin.x + 16.0f, canvas.origin.y + 16.0f), ImGui::GetColorU32(ImGuiCol_TextDisabled), "Right-click to add a state");
    canvas.End();

    if (canvas.RightClicked() && m_Drag != DragKind::Transition)
    {
        m_ContextPoint = canvas.MouseInGraph();
        m_ContextTarget = stateAt(mouse);
        ImGui::OpenPopup(m_ContextTarget >= -1 ? "##statemenu" : "##machinemenu");
    }
    if (ImGui::BeginPopup("##statemenu"))
    {
        const int target = m_ContextTarget;
        if (target >= -1 && target < states)
        {
            ImGui::TextDisabled("%s", target == -1 ? "Any State" : machine.states[target].c_str());
            if (ImGui::MenuItem("Make transition"))
            {
                m_Drag = DragKind::Transition;
                m_DragIndex = target;
            }
            if (target >= 0)
            {
                if (ImGui::MenuItem("Set as entry", nullptr, machine.entryState == target))
                    Apply("Set entry state", [&](AnimationGraph& g) { g.nodes[machineIndex].entryState = target; });
                if (ImGui::MenuItem("Delete state", "Del"))
                {
                    Apply("Delete state", [&](AnimationGraph& g) { g.nodes[machineIndex].RemoveState(target); });
                    m_SelectedState = -1;
                }
            }
        }
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("##machinemenu"))
    {
        const glm::vec2 at = m_ContextPoint;
        auto addState = [&](int motion, const std::string& base) {
            std::string name = base.empty() ? "State" : base;
            for (int n = 2; machine.FindState(name) >= 0; ++n)
                name = (base.empty() ? std::string("State") : base) + " " + std::to_string(n);
            Apply("Add state", [&](AnimationGraph& g) { g.nodes[machineIndex].AddState(name, motion, at); });
            m_SelectedState = static_cast<int>(G().nodes[machineIndex].states.size()) - 1;
        };
        if (ImGui::MenuItem("New state with a clip"))
        {
            // A clip node for it, left of the machine on the graph.
            Apply("Add state", [&](AnimationGraph& g) {
                const int clip = NewNode(AnimationNodeType::Clip, g.nodes[machineIndex].position + glm::vec2(-260.0f, static_cast<float>(g.nodes[machineIndex].states.size()) * 90.0f));
                AnimationNode& m = g.nodes[machineIndex];
                std::string name = "State";
                for (int n = 2; m.FindState(name) >= 0; ++n)
                    name = "State " + std::to_string(n);
                g.nodes[clip].name = UniqueNodeName(name + " clip");
                m.AddState(name, clip, at);
            });
            m_SelectedState = static_cast<int>(G().nodes[machineIndex].states.size()) - 1;
        }
        if (ImGui::BeginMenu("New state playing"))
        {
            for (int n = 0; n < static_cast<int>(graph.nodes.size()); ++n)
            {
                if (n == machineIndex || Feeds(graph, machineIndex, n))
                    continue;
                ImGui::PushID(n);
                if (ImGui::MenuItem(graph.nodes[n].name.c_str()))
                    addState(n, graph.nodes[n].name);
                ImGui::PopID();
            }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }

    if (ImGui::IsWindowFocused() && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete))
    {
        if (m_SelectedTransition >= 0)
        {
            const int t = m_SelectedTransition;
            Apply("Delete transition", [&](AnimationGraph& g) { auto& list = g.nodes[machineIndex].transitions; list.erase(list.begin() + t); });
            m_SelectedTransition = -1;
        }
        else if (m_SelectedState >= 0)
        {
            const int s = m_SelectedState;
            Apply("Delete state", [&](AnimationGraph& g) { g.nodes[machineIndex].RemoveState(s); });
            m_SelectedState = -1;
        }
    }
    if (ImGui::IsWindowFocused() && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Backspace) && m_Drag == DragKind::None)
        m_View = -1;
}
