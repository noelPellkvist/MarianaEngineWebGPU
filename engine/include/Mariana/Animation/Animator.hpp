#pragma once
#include <Mariana/Scene/ECS.hpp>
#include <Mariana/Animation/AnimationClip.hpp>
#include <Mariana/Animation/AnimationGraph.hpp>
#include <Mariana/Animation/Pose.hpp>
#include <Mariana/Animation/Skeleton.hpp>
#include <Mariana/Assets/AssetManager.hpp>
#include <memory>
#include <string_view>
#include <vector>

// Animates a model: goes on its root (the glTF loader's wrapper entity) and moves the bones below
// it, the entities whose AnimationTargetEntity is a bone of `skeleton`. With a `graph`, the graph
// mixes its clips into the pose (its parameters set with SetFloat/SetBool/SetTrigger); without one,
// the clips started with Play and CrossFade do. The pose is written to the bones' LocalTRS: only the
// channels some clip animates, and only values that changed. Other models below it (another
// Animator, or an entity that isn't a bone) are left alone.
struct Animator
{
    AssetHandle<Skeleton> skeleton;
    std::vector<AssetHandle<AnimationClip>> clips;
    AssetHandle<AnimationGraph> graph;
    float speed = 1.0f;
    bool playing = true;

    Animator() = default;
    // Copies the settings, parameters and what plays; the bones it found belong to the source's
    // entities.
    Animator(const Animator& other);
    Animator& operator=(const Animator& other);
    Animator(Animator&&) = default;
    Animator& operator=(Animator&&) = default;

    // Animator and AnimationTargetEntity; call before Scene::UpdateComponentRegistry.
    static void RegisterComponents();
    // Updates every Animator of the scene by the delta given to System::Run (game time).
    static System CreateSystem(Scene& scene);

    // Clips without a graph. Play starts clips[clip] from the start at once; CrossFade fades it in
    // over `seconds` while what played fades out (and keeps playing until it's gone).
    int FindClip(std::string_view name) const;
    void Play(int clip, bool loop = true);
    void CrossFade(int clip, float seconds, bool loop = true);

    // The clip played or faded to last.
    int CurrentClip() const;
    const std::shared_ptr<AnimationClip>& GetCurrentClip() const;
    float Time() const;
    void SetTime(float time);
    bool Loop() const;
    void SetLoop(bool loop);

    // Each clip still in the mix, oldest first. `fade` goes 0 -> 1 while it fades in; `weight` is its
    // share of the pose.
    struct ClipState
    {
        int clip = -1;
        float time = 0.0f;
        bool loop = true;
        float fade = 1.0f;
        float fadeDuration = 0.0f;
        float weight = 1.0f;
    };
    const std::vector<ClipState>& Playing() const { return m_Playing; }

    // Graph parameters by name (unknown names are ignored, and read 0), or by index into the graph's.
    void SetFloat(std::string_view name, float value);
    void SetBool(std::string_view name, bool value);
    void SetTrigger(std::string_view name);
    void ResetTrigger(std::string_view name);
    float GetFloat(std::string_view name) const;
    bool GetBool(std::string_view name) const;
    float GetParameter(int index) const;
    void SetParameter(int index, float value);

    // Per graph node, as of the last update: its share of the pose, and for clips the clip
    // (index into `clips`, -1 = not found) and its time.
    struct NodeState
    {
        float weight = 0.0f;
        float time = 0.0f;
        int clip = -1;
    };
    const std::vector<NodeState>& Nodes() const { return m_Nodes; }

    // Per graph node; for state machines, the states in the mix, oldest first. The newest is the
    // current state: `fade` goes 0 -> 1 while its transition runs, `weight` is each one's share and
    // `normalizedTime` how far its clips got (1 = one loop of them).
    struct ActiveState
    {
        int state = 0;
        float fade = 1.0f;
        float fadeDuration = 0.0f;
        float weight = 1.0f;
        float normalizedTime = 0.0f;
        bool interruptible = true;
    };
    struct MachineState
    {
        std::vector<ActiveState> active;
    };
    const std::vector<MachineState>& Machines() const { return m_Machines; }

    // Moves the first state machine with a state of that name to it, cross-fading over `seconds`
    // (0 = at once), whatever its transitions say. False when no state has the name.
    bool CrossFadeState(std::string_view state, float seconds);

    // Advances time by dt (if playing) and poses the bones below `root`.
    void Update(Entity root, float dt);

    // The last pose written, by skeleton bone.
    const Pose& GetPose() const { return m_Pose; }
    // Bones that have an entity below the root, and tracks of the current clip that found a bone.
    size_t BoundBones() const { return m_BoundBones; }
    size_t BoundTracks() const;

private:
    struct SyncGroup
    {
        std::string name;
        std::vector<int> members; // clip nodes
        float phase = 0.0f;       // normalized time
    };

    bool NeedsBind() const;
    void Bind(Entity root);
    void BindGraph(const AnimationGraph& definition);
    void EnsureParameters() const;

    void AdvanceClips(float dt);
    void AdvanceGraph(float dt);
    void UpdateMachine(int node, float delta);
    void StartState(int node, int state, float duration, bool interruptible);
    void EnterState(int node, int state);
    bool ConditionsHold(const AnimationTransition& transition) const;
    void Weigh(int node, float weight);
    float WeightOf(const AnimationWeight& weight) const;
    void Segment(const AnimationNode& node, int& a, int& b, float& t) const;
    void SampleInto(int clip, float time, Pose& pose) const;
    void EvaluateClips();
    void EvaluateNode(int node, Pose& out, size_t depth);
    Pose& Scratch(size_t depth);
    bool StateChanged(bool useGraph);
    void WriteBones();

    std::vector<ClipState> m_Playing;
    mutable const AnimationGraph* m_ParameterGraph = nullptr;
    mutable std::vector<float> m_Parameters;

    // Found by Bind for the current skeleton, clips and graph (not copied).
    const Skeleton* m_BoundSkeleton = nullptr;
    const AnimationGraph* m_BoundGraph = nullptr;
    std::vector<const AnimationClip*> m_BoundClips;
    std::vector<ClipBinding> m_Bindings; // per clip
    std::vector<Entity> m_Bones;         // per bone, invalid where the bone has no entity
    std::vector<uint8_t> m_Channels;     // per bone, which of T/R/S some clip animates
    size_t m_BoundBones = 0;
    std::vector<NodeState> m_Nodes;
    std::vector<int> m_SyncGroupOf;      // per node, -1 = none
    std::vector<SyncGroup> m_SyncGroups;
    std::vector<std::vector<float>> m_Masks; // per Layer node
    std::vector<Pose> m_References;          // per additive node
    std::vector<MachineState> m_Machines;    // per state machine node
    std::vector<std::vector<std::vector<int>>> m_StateNodes; // per state machine node, per state: the nodes below it
    std::vector<std::pair<std::string, float>> m_PendingStates; // CrossFadeState before the graph was bound
    std::vector<Pose> m_Scratch;
    Pose m_Pose;
    std::vector<float> m_State;   // what the pose was made from (clips, times, parameters)
    std::vector<float> m_Applied; // the same for the pose written last
    bool m_Bound = false;
    bool m_GraphValid = false;
};
