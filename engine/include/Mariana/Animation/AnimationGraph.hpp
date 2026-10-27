#pragma once
#include <Mariana/Animation/AnimationClip.hpp>
#include <Mariana/Assets/AssetManager.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <glm/vec2.hpp>

class Skeleton;

// A moment of a clip game code hears of ("Footstep" at 0.4 s): Animator::Events, and a script's
// OnAnimationEvent on the Animator's entity.
struct AnimationEvent
{
    float time = 0.0f; // seconds into the clip
    std::string name;
};

// A clip under the name graphs refer to it by (what an Animator and a graph hold), with its events
// (kept where the clip is named: a scene's Animator, an .animgraph file).
struct NamedClip
{
    std::string name;
    AssetHandle<AnimationClip> clip;
    std::vector<AnimationEvent> events;
};

enum class AnimationParameterType { Float, Bool, Trigger };

// A value the game sets on an Animator (Animator::SetFloat/SetBool/SetTrigger) that the graph reads.
struct AnimationParameter
{
    std::string name;
    AnimationParameterType type = AnimationParameterType::Float;
    float defaultValue = 0.0f; // bools and triggers: 0 or 1
};

// How much of a node's second input there is: a constant, or a float parameter's value. Either is
// clamped to 0..1.
struct AnimationWeight
{
    float value = 1.0f;
    int parameter = -1;
};

// The bones a layer applies to, by name. A listed bone and everything below it get the entry's
// weight; an entry further down overrides it for its own subtree. Bones below no entry get 0, and an
// empty mask is every bone at 1.
struct BoneMask
{
    struct Entry
    {
        std::string bone;
        float weight = 1.0f;
    };
    std::vector<Entry> entries;

    // A weight per bone of `skeleton`; entries naming no bone of it are ignored.
    std::vector<float> Resolve(const Skeleton& skeleton) const;
};

enum class AnimationNodeType { Clip, Blend, Blend1D, Blend2D, Additive, Layer, StateMachine };
enum class LayerMode { Override, Additive };
// What an additive input is relative to: its clip's first frame (only for a Clip input), or the
// skeleton's rest pose.
enum class AdditiveReference { FirstFrame, Rest };

// A test of a parameter: If/IfNot for bools and triggers (a trigger is reset by the transition that
// uses it), the others compare a float with `threshold`.
enum class ConditionMode { If, IfNot, Greater, Less, Equals, NotEquals };
struct AnimationCondition
{
    int parameter = -1;
    ConditionMode mode = ConditionMode::If;
    float threshold = 0.0f;
};

// When a state machine moves from one state to another, and how long the cross-fade takes.
struct AnimationTransition
{
    int from = -1; // a state of the machine; -1 = from any state (but not to itself)
    int to = 0;
    float duration = 0.25f;
    // Not before the state it leaves has played this much of its normalized time (1 = the end of its
    // first loop); < 0 = whenever the conditions hold.
    float exitTime = -1.0f;
    // While it fades, transitions out of `to` (and from any state) may start; otherwise none can
    // until it's done.
    bool interruptible = false;
    // All must hold. A transition needs conditions, an exit time or both.
    std::vector<AnimationCondition> conditions;
};

// One node of a graph. Which fields matter depends on its type.
struct AnimationNode
{
    AnimationNodeType type = AnimationNodeType::Clip;
    std::string name;
    std::vector<int> inputs; // node indices

    // Clip: a clip of the Animator, by name.
    std::string clip;
    float speed = 1.0f;
    bool loop = true;
    // Clips of one group advance by normalized time, at the rate their weights blend to, so they
    // stay in step (a walk and a run keep their feet together).
    std::string syncGroup;

    // Blend (a, b), Additive (base, additive) and Layer (base, layer): how much of the second input.
    AnimationWeight weight;

    // Blend1D: the inputs placed along a float parameter, at ascending thresholds (one per input).
    int parameter = -1;
    std::vector<float> thresholds;
    // Blend2D: the inputs placed at points on two float parameters (`parameter` across, `parameterY`
    // up), weighted by gradient bands (freeform cartesian).
    int parameterY = -1;
    std::vector<glm::vec2> positions;

    // Layer: the layer replaces the base (Override) or is added to it (Additive), on the masked bones.
    LayerMode layerMode = LayerMode::Override;
    BoneMask mask;
    // Additive, and Layer in Additive mode.
    AdditiveReference reference = AdditiveReference::FirstFrame;

    // StateMachine: a state per input, named by `states`, starting in `entryState`. Transitions are
    // tried in order each update; the first whose conditions hold starts. Entering a state plays its
    // clips (and state machines) from the start.
    std::vector<std::string> states;
    std::vector<AnimationTransition> transitions;
    int entryState = 0;

    // Where an editor draws it: the node on the graph's canvas, and a state machine's states and its
    // "any state" box in the machine's own view.
    glm::vec2 position{ 0.0f };
    std::vector<glm::vec2> statePositions;
    glm::vec2 anyStatePosition{ -220.0f, 0.0f };

    int FindState(std::string_view state) const;
    // A state machine's states: adding one (playing `node`) or removing one with its transitions
    // (later states move down; the entry state follows).
    int AddState(std::string state, int node, glm::vec2 position = glm::vec2(0.0f));
    void RemoveState(int state);
};

// How an Animator mixes its clips: parameters, and nodes whose `output` is the pose. Plain data,
// so code (Builder), files (AnimationGraphFile) and editors make the same thing. Clip nodes name a
// clip: one of the Animator's, else one the graph brings in `clips` (from other files, bound to the
// model's bones by name), so one graph serves every model with those bones. An asset
// (AssetType::AnimationGraph).
class AnimationGraph
{
public:
    std::vector<AnimationParameter> parameters;
    std::vector<AnimationNode> nodes;
    int output = -1;
    std::vector<NamedClip> clips;

    int FindParameter(std::string_view name) const;
    int FindNode(std::string_view name) const;
    // Inputs, parameters and thresholds valid and no cycles; `error` says what isn't.
    bool Validate(std::string& error) const;

    // Editing keeps references intact: removing a node or parameter disconnects what used it (-1)
    // and renumbers the later ones. Call MarkChanged after any edit, so Animators using the graph
    // bind it again (Revision).
    int AddNode(AnimationNode node);
    void RemoveNode(int node);
    void RemoveParameter(int parameter);
    void MarkChanged() { ++m_Revision; }
    uint32_t Revision() const { return m_Revision; }
    // Becomes a copy of `other` (an undo step, say) as a new revision of this graph.
    void ReplaceWith(const AnimationGraph& other)
    {
        const uint32_t revision = m_Revision;
        *this = other;
        m_Revision = revision + 1;
    }

    class Builder;

private:
    uint32_t m_Revision = 0;
};

// Builds a graph in code; each node function returns the node's index.
//   AnimationGraph::Builder b;
//   b.Float("speed");
//   int move = b.Blend1D("speed", { { b.Clip("Idle"), 0.0f }, { b.Clip("Walk", { 1.0f, true, "move" }), 0.5f },
//                                   { b.Clip("Run", { 1.0f, true, "move" }), 1.0f } });
//   b.Output(b.Layer(move, b.Clip("Wave"), BoneMask{ { { "Spine1" } } }));
//   std::shared_ptr<AnimationGraph> graph = b.Build();
// State machines name their states and add transitions by those names:
//   b.Bool("grounded", true); b.Trigger("jump");
//   int body = b.StateMachine({ { "Move", move }, { "Jump", b.Clip("Jump", { 1.0f, false }) } });
//   b.Transition(body, "", "Jump", 0.1f, { b.When("jump") });
//   b.Transition(body, "Jump", "Move", 0.2f, { b.When("grounded") }).exitTime = 0.8f;
class AnimationGraph::Builder
{
public:
    int Float(std::string name, float value = 0.0f);
    int Bool(std::string name, bool value = false);
    int Trigger(std::string name);
    // A weight read from a parameter added before.
    AnimationWeight Parameter(std::string_view name) const;

    struct ClipOptions
    {
        float speed = 1.0f;
        bool loop = true;
        std::string syncGroup;
    };
    int Clip(std::string clip);
    int Clip(std::string clip, ClipOptions options);
    int Blend(int a, int b, AnimationWeight weight);
    int Blend1D(std::string_view parameter, std::vector<std::pair<int, float>> inputs);
    int Blend2D(std::string_view x, std::string_view y, std::vector<std::pair<int, glm::vec2>> inputs);
    int Additive(int base, int additive, AnimationWeight weight = {}, AdditiveReference reference = AdditiveReference::FirstFrame);
    int Layer(int base, int layer, BoneMask mask, AnimationWeight weight = {}, LayerMode mode = LayerMode::Override);
    // States as (name, node).
    int StateMachine(std::vector<std::pair<std::string, int>> states, int entryState = 0);
    // A transition of `machine` between states named `from` ("" = any state) and `to`; tweak the
    // returned one for an exit time or interruptions.
    AnimationTransition& Transition(int machine, std::string_view from, std::string_view to, float duration,
                                    std::vector<AnimationCondition> conditions = {});
    // A condition on a parameter added before.
    AnimationCondition When(std::string_view parameter, ConditionMode mode = ConditionMode::If, float threshold = 0.0f) const;

    // Nodes are named after their clip or type; Name changes that.
    Builder& Name(int node, std::string name);
    // The last node added is the output unless this says otherwise.
    Builder& Output(int node);
    // A clip the graph brings, for Clip nodes naming `name`.
    Builder& AddClip(std::string name, AssetHandle<AnimationClip> clip);

    // Null (and the reason logged) when the graph isn't valid.
    std::shared_ptr<AnimationGraph> Build();

private:
    int Add(AnimationNode node);

    AnimationGraph m_Graph;
    std::string m_Error;
};
