#pragma once
#include <Mariana/Scene/ECS.hpp>
#include <Mariana/Animation/AnimationClip.hpp>
#include <Mariana/Animation/AnimationGraph.hpp>
#include <Mariana/Animation/Pose.hpp>
#include <Mariana/Animation/Skeleton.hpp>
#include <Mariana/Assets/AssetManager.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <glm/glm.hpp>

// Where an Animator's root motion goes when something else moves its entity: the physics world
// puts one on the Animator of a character, and the Animator then adds each update's motion here
// instead of moving the entity; the character walks it and takes it (zeroes it). A runtime component.
struct RootMotion
{
    glm::vec3 translation{ 0.0f }; // in the entity's own space (the model's), since it was last taken
    float yaw = 0.0f;              // radians about +Y
    float seconds = 0.0f;          // the game time it took
};

// Animates a model: goes on its root (the glTF loader's wrapper entity) and moves the bones below
// it, the entities whose AnimationTargetEntity is a bone of `skeleton`. With a `graph`, the graph
// mixes its clips into the pose (its parameters set with SetFloat/SetBool/SetTrigger); without one,
// the clips started with Play and CrossFade do. The pose is written to the bones' LocalTRS: only the
// channels some clip animates, and only values that changed. Other models below it (another
// Animator, or an entity that isn't a bone) are left alone.
struct Animator
{
    AssetHandle<Skeleton> skeleton;
    // Its own clips, by the names graphs use (the glTF loader names them like the file's animations).
    std::vector<NamedClip> clips;
    AssetHandle<AnimationGraph> graph;
    float speed = 1.0f;
    bool playing = true;
    // Root motion: the root bone's walk (horizontal movement) and turn (yaw) are taken out of the
    // pose and move the entity instead (or a physics character, through RootMotion), so a walk
    // clip walks the model rather than sliding its hips away from it. Its height stays in the pose.
    bool rootMotion = false;
    // The bone it comes from; empty = the first bone named like hips or pelvis that a clip moves or
    // turns, else the first one a clip moves. The update that binds the clips doesn't move the
    // entity (a physics character takes the motion from the next one on).
    std::string rootBone;

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

    // A clip by name: its own first, then the graph's (indices past `clips`). -1 when there is none.
    int FindClip(std::string_view name) const;
    // Its clips and then the graph's: what FindClip and NodeState::clip index.
    int ClipCount() const;
    const NamedClip* GetClip(int index) const;

    // Clips without a graph. Play starts clips[clip] from the start at once; CrossFade fades it in
    // over `seconds` while what played fades out (and keeps playing until it's gone).
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
    // Shows a state at a normalized time (0..1 = its first loop) without playing: it becomes the
    // current state at once, and the machine holds it (no transitions) until the game plays.
    bool PreviewState(std::string_view state, float normalizedTime);

    // Advances time by dt (if playing) and poses the bones below `root`.
    void Update(Entity root, float dt);

    // Why its graph isn't used (it doesn't validate), or empty.
    const std::string& GraphError() const { return m_GraphError; }

    // The last pose written, by skeleton bone.
    const Pose& GetPose() const { return m_Pose; }
    // Root motion of the last update: the speed (m/s in the model's space, before its scale) and
    // the turn (radians per second about +Y). The bone it comes from, -1 = none.
    glm::vec3 RootVelocity() const { return m_RootVelocity; }
    float RootTurnRate() const { return m_RootTurnRate; }
    int RootBone() const { return m_RootBone; }
    // Writes the whole pose to the bones at the next update even if nothing changed (after
    // something else moved them, a ragdoll say).
    void Refresh() { m_Refresh = true; }
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
    void SetStateTime(int node, int state, float normalizedTime);
    void Weights2D(int node);
    bool ConditionsHold(const AnimationTransition& transition) const;
    // `motion`: the share of the root motion (none from a layer's or an additive's second input).
    void Weigh(int node, float weight, float motion);
    float WeightOf(const AnimationWeight& weight) const;
    void Segment(const AnimationNode& node, int& a, int& b, float& t) const;
    void SampleInto(int clip, float time, Pose& pose) const;
    // Root motion: the bone and what it is measured against; a clip's root at a time (in model
    // space, its yaw from the rest pose); taking it out of a pose; adding a clip's move to the
    // update's; moving the entity (or filling its RootMotion).
    void ResolveRootBone();
    bool RootAt(int clip, float time, glm::vec3& position, float& yaw) const;
    void StripRoot(int clip, Pose& pose) const;
    void AddRootMotion(int clip, float from, float to, int wraps, float weight);
    void ApplyRootMotion(Entity root, float dt);
    void EvaluateClips();
    void EvaluateNode(int node, Pose& out, size_t depth);
    Pose& Scratch(size_t depth);
    bool StateChanged(bool useGraph);
    void WriteBones();

    std::vector<ClipState> m_Playing;
    mutable const AnimationGraph* m_ParameterGraph = nullptr;
    mutable uint32_t m_ParameterRevision = 0;
    mutable std::vector<std::string> m_ParameterNames;
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
    std::vector<float> m_MotionWeights; // per node, its share of the root motion
    std::vector<float> m_OldTimes;      // per node, its time before this update advanced it
    std::vector<int> m_SyncGroupOf;      // per node, -1 = none
    std::vector<SyncGroup> m_SyncGroups;
    std::vector<std::vector<float>> m_Masks; // per Layer node
    std::vector<Pose> m_References;          // per additive node
    std::vector<MachineState> m_Machines;    // per state machine node
    std::vector<std::vector<std::vector<int>>> m_StateNodes; // per state machine node, per state: the nodes below it
    std::vector<std::vector<float>> m_BlendWeights; // per Blend2D node, a weight per input
    struct PendingState
    {
        std::string state;
        float seconds = 0.0f;
        float normalizedTime = -1.0f; // PreviewState
    };
    std::vector<PendingState> m_PendingStates; // asked for before the graph was bound
    bool m_Preview = false;
    bool m_Refresh = false;
    std::vector<Pose> m_Scratch;
    Pose m_Pose;
    std::vector<float> m_State;   // what the pose was made from (clips, times, parameters)
    std::vector<float> m_Applied; // the same for the pose written last
    bool m_Bound = false;
    bool m_GraphValid = false;
    // Root motion, found by Bind: the bone, the rest transform of the bones above it (model space)
    // and its own rest pose in model space; per clip, its track for the bone (-1 = none).
    int m_RootBone = -1;
    std::string m_BoundRootBone;
    glm::mat4 m_RootParent{ 1.0f };
    glm::mat4 m_RootParentInverse{ 1.0f };
    glm::quat m_RootParentRotation{ 1.0f, 0.0f, 0.0f, 0.0f };
    glm::vec3 m_RootRestPosition{ 0.0f };
    glm::quat m_RootRestRotation{ 1.0f, 0.0f, 0.0f, 0.0f };
    std::vector<int> m_RootTracks;
    glm::vec3 m_RootDelta{ 0.0f }; // this update's, in the model's space
    float m_RootYaw = 0.0f;
    glm::vec3 m_RootVelocity{ 0.0f };
    float m_RootTurnRate = 0.0f;
    uint32_t m_BoundRevision = 0;
    std::string m_GraphError;
};
