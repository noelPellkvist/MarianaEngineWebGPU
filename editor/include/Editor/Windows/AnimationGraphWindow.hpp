#pragma once

#include <Editor/Windows/EditorWindow.hpp>
#include <Editor/Windows/GraphCanvas.hpp>
#include <Mariana/Animation/AnimationGraph.hpp>
#include <Mariana/Scene/ECS.hpp>

#include <filesystem>
#include <map>
#include <string>

class UndoStack;
struct Animator;

// Edits an AnimationGraph asset. The graph's nodes are boxes on a canvas: drag one to move it, drag
// from its output (right) to another's input (left) to connect them, right-click to add nodes;
// double-click a state machine to see its states and transitions (right-click a state to make a
// transition). Side panels hold the parameters, the clips the graph brings and the selected node,
// state or transition. Edits change the asset in place, so Animators using it follow at once, and
// are undoable; Save writes its .animgraph file. While the selected entity's Animator uses the
// graph, its live weights, states and parameters show.
class AnimationGraphWindow final : public EditorWindow
{
public:
    AnimationGraphWindow(GUI& gui, AssetManager2& assets, UndoStack& undo, const Scene& scene, std::filesystem::path projectDir);

    void Draw() override;

    void Open(AssetHandle<AnimationGraph> graph);
    const AssetHandle<AnimationGraph>& Graph() const { return m_Graph; }
    // Writes a new graph file and loads it: an empty one, or for `animator` a state machine with a
    // state per clip. `file` gets a number if it exists. Empty on failure (logged).
    AssetHandle<AnimationGraph> CreateGraph(std::filesystem::path file, const Animator* animator);
    // The entity whose Animator shows live (it or the first ancestor with an Animator using the graph).
    void SetPreview(Entity entity) { m_PreviewEntity = entity; }

private:
    struct NodeLayout
    {
        glm::vec2 min{ 0.0f }, max{ 0.0f };
        glm::vec2 output{ 0.0f };
        std::vector<glm::vec2> inputs;
    };
    enum class DragKind { None, Node, Link, State, Transition };

    AnimationGraph& G() { return *m_Graph.Get(); }
    Animator* PreviewAnimator();

    // Edits: Apply does one at once (an undo step); a value being dragged or typed is Begun on its
    // first change and committed as one step when no widget is held any more.
    template <typename Fn>
    void Apply(const char* name, Fn&& fn);
    void Begin(const char* name);
    void Changed();
    void CommitIfIdle(bool force = false);
    void PushStep(const std::string& name, const AnimationGraph& before);

    bool Save();
    void DrawToolbar();
    void DrawSidePanel();
    void DrawParameters();
    void DrawClips();
    void DrawSelection();
    void DrawNodeProperties(int index);
    void DrawStateProperties(int machine, int state);
    void DrawTransitionProperties(int machine, int transition);
    bool ParameterCombo(const char* label, int& parameter, bool floatsOnly, bool allowNone);

    void DrawGraphView();
    void DrawMachineView(int machine);
    NodeLayout Layout(int index) const;
    void ArrangeGraph();
    void ArrangeMachine(int machine);
    // Zooms and pans `canvas` so the boxes from `min` to `max` (graph units) fit it.
    static void Frame(GraphCanvas& canvas, glm::vec2 min, glm::vec2 max);
    void AddNodeMenu(glm::vec2 at);
    int NewNode(AnimationNodeType type, glm::vec2 at);
    std::string UniqueNodeName(const std::string& base) const;

    AssetManager2& m_Assets;
    UndoStack& m_Undo;
    const Scene& m_Scene;
    std::filesystem::path m_ProjectDir;
    AssetHandle<AnimationGraph> m_Graph;
    Entity m_PreviewEntity;

    GraphCanvas m_GraphCanvas;
    std::map<int, GraphCanvas> m_MachineCanvases;
    int m_View = -1; // the state machine node shown, -1 = the graph

    int m_SelectedNode = -1;
    int m_SelectedState = -1;
    int m_SelectedTransition = -1;
    bool m_SelectedAnyState = false;

    DragKind m_Drag = DragKind::None;
    int m_DragIndex = -1;            // node or state being moved; link/transition source
    int m_LinkInput = -1;            // a link dragged from (or off) an input: its slot, else -1
    bool m_LinkFromInput = false;    // dragged from an unconnected input toward an output
    glm::vec2 m_DragOffset{ 0.0f };
    glm::vec2 m_ContextPoint{ 0.0f };
    int m_ContextTarget = -1;        // node or state right-clicked (-2 = the any state box)

    bool m_Pending = false;
    std::string m_PendingName;
    AnimationGraph m_Before;
    std::string m_Status;
    bool m_NeedsLayout = false;
    bool m_FrameGraph = true;        // fit the graph's view to its nodes on the next draw
};
