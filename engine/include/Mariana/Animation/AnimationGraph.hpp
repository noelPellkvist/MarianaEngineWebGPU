#pragma once
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class Skeleton;

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

enum class AnimationNodeType { Clip, Blend, Blend1D, Additive, Layer };
enum class LayerMode { Override, Additive };
// What an additive input is relative to: its clip's first frame (only for a Clip input), or the
// skeleton's rest pose.
enum class AdditiveReference { FirstFrame, Rest };

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

    // Layer: the layer replaces the base (Override) or is added to it (Additive), on the masked bones.
    LayerMode layerMode = LayerMode::Override;
    BoneMask mask;
    // Additive, and Layer in Additive mode.
    AdditiveReference reference = AdditiveReference::FirstFrame;
};

// How an Animator mixes its clips: parameters, and nodes whose `output` is the pose. Plain data,
// so code (Builder), files and editors make the same thing. Clips are referred to by name, so one
// graph serves every model whose Animator has clips of those names. An asset (AssetType::AnimationGraph).
class AnimationGraph
{
public:
    std::vector<AnimationParameter> parameters;
    std::vector<AnimationNode> nodes;
    int output = -1;

    int FindParameter(std::string_view name) const;
    int FindNode(std::string_view name) const;
    // Inputs, parameters and thresholds valid and no cycles; `error` says what isn't.
    bool Validate(std::string& error) const;

    class Builder;
};

// Builds a graph in code; each node function returns the node's index.
//   AnimationGraph::Builder b;
//   b.Float("speed");
//   int move = b.Blend1D("speed", { { b.Clip("Idle"), 0.0f }, { b.Clip("Walk", { 1.0f, true, "move" }), 0.5f },
//                                   { b.Clip("Run", { 1.0f, true, "move" }), 1.0f } });
//   b.Output(b.Layer(move, b.Clip("Wave"), BoneMask{ { { "Spine1" } } }));
//   std::shared_ptr<AnimationGraph> graph = b.Build();
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
    int Additive(int base, int additive, AnimationWeight weight = {}, AdditiveReference reference = AdditiveReference::FirstFrame);
    int Layer(int base, int layer, BoneMask mask, AnimationWeight weight = {}, LayerMode mode = LayerMode::Override);

    // Nodes are named after their clip or type; Name changes that.
    Builder& Name(int node, std::string name);
    // The last node added is the output unless this says otherwise.
    Builder& Output(int node);

    // Null (and the reason logged) when the graph isn't valid.
    std::shared_ptr<AnimationGraph> Build();

private:
    int Add(AnimationNode node);

    AnimationGraph m_Graph;
};
