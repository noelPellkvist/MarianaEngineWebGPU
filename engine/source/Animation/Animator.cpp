#include <Mariana/Animation/Animator.hpp>
#include <Mariana/Foundation/Logger.hpp>
#include <Mariana/Foundation/Profiler.hpp>
#include <Mariana/Scene/Transform.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace
{
    enum Channel : uint8_t { kTranslation = 1, kRotation = 2, kScale = 4 };

    // Writes `count` floats only if they differ, so unchanged channels don't dirty the transform.
    bool Assign(float* dst, const float* src, int count)
    {
        bool changed = false;
        for (int i = 0; i < count; ++i)
        {
            if (dst[i] != src[i])
            {
                dst[i] = src[i];
                changed = true;
            }
        }
        return changed;
    }

    // A time moved by `delta`, wrapped (looping) or held at the ends.
    float Advance(float time, float delta, float duration, bool loop)
    {
        time += delta;
        if (duration <= 0.0f)
            return 0.0f;
        if (loop)
        {
            time = std::fmod(time, duration);
            return time < 0.0f ? time + duration : time;
        }
        return std::clamp(time, 0.0f, duration);
    }

    // The turn about +Y in a rotation (its twist), in radians.
    float YawOf(const glm::quat& q)
    {
        return 2.0f * std::atan2(q.y, q.w);
    }

    float WrapAngle(float angle)
    {
        return std::remainder(angle, glm::two_pi<float>());
    }

    glm::mat4 Local(const BoneTransform& t)
    {
        return glm::translate(glm::mat4(1.0f), t.translation) * glm::mat4_cast(t.rotation) * glm::scale(glm::mat4(1.0f), t.scale);
    }

    // How many times a looping clip went past its end (or back past its start) moving from -> to.
    int Wraps(float from, float to, float step, bool loop)
    {
        if (!loop || step == 0.0f)
            return 0;
        if (step > 0.0f && to < from)
            return 1;
        if (step < 0.0f && to > from)
            return -1;
        return 0;
    }

    // A fade stack (clips cross-faded, or a state machine's states): the newest, last, fades in
    // over the rest; the ones it interrupted stay where they got to.
    template <typename Entry>
    void FadeWeights(std::vector<Entry>& stack, float delta)
    {
        Entry& newest = stack.back();
        if (newest.fade < 1.0f)
            newest.fade = newest.fadeDuration > 0.0f ? std::min(1.0f, newest.fade + std::abs(delta) / newest.fadeDuration) : 1.0f;

        float remaining = 1.0f;
        for (size_t i = stack.size(); i-- > 0;)
        {
            const float fade = i == 0 ? 1.0f : stack[i].fade;
            stack[i].weight = remaining * fade;
            remaining *= 1.0f - fade;
        }
    }

    // Entries under one that has fully faded in no longer show.
    template <typename Entry>
    void DropFaded(std::vector<Entry>& stack)
    {
        stack.erase(std::remove_if(stack.begin(), stack.end() - 1, [](const Entry& e) { return e.weight <= 0.0f; }), stack.end() - 1);
    }

    template <typename Entry>
    void FadeIn(std::vector<Entry>& stack, float delta)
    {
        FadeWeights(stack, delta);
        DropFaded(stack);
    }
}

void Animator::RegisterComponents()
{
    ECS::RegisterComponent<AnimationTargetEntity>("AnimationTargetEntity");
    ECS::RegisterComponent<Animator>("Animator");
    ECS::RegisterRuntimeComponent<RootMotion>("RootMotion");
}

System Animator::CreateSystem(Scene& scene)
{
    // Inactive models (or below an inactive entity) hold their pose.
    return scene.CreateSystem<Animator>([](Entity e, Animator& animator, float dt) {
        if (e.IsActive())
            animator.Update(e, dt);
    });
}

Animator::Animator(const Animator& other)
    : skeleton(other.skeleton),
      clips(other.clips),
      graph(other.graph),
      speed(other.speed),
      playing(other.playing),
      rootMotion(other.rootMotion),
      rootBone(other.rootBone),
      m_Playing(other.m_Playing),
      m_ParameterGraph(other.m_ParameterGraph),
      m_ParameterRevision(other.m_ParameterRevision),
      m_ParameterNames(other.m_ParameterNames),
      m_Parameters(other.m_Parameters),
      m_Vectors(other.m_Vectors),
      m_PendingStates(other.m_PendingStates)
{
}

Animator& Animator::operator=(const Animator& other)
{
    if (this != &other)
        *this = Animator(other);
    return *this;
}

int Animator::FindClip(std::string_view name) const
{
    for (int i = 0; i < ClipCount(); ++i)
        if (GetClip(i)->name == name)
            return i;
    return -1;
}

int Animator::ClipCount() const
{
    const AnimationGraph* definition = graph.Get().get();
    return static_cast<int>(clips.size() + (definition ? definition->clips.size() : 0));
}

const NamedClip* Animator::GetClip(int index) const
{
    if (index < 0)
        return nullptr;
    if (index < static_cast<int>(clips.size()))
        return &clips[index];
    const AnimationGraph* definition = graph.Get().get();
    const size_t own = clips.size();
    return definition && static_cast<size_t>(index) - own < definition->clips.size() ? &definition->clips[index - own] : nullptr;
}

void Animator::Play(int clip, bool loop)
{
    if (clip < 0 || clip >= static_cast<int>(clips.size()))
        return;

    m_Playing.assign(1, ClipState{ clip, 0.0f, loop, 1.0f, 0.0f, 1.0f });
    playing = true;
}

void Animator::CrossFade(int clip, float seconds, bool loop)
{
    if (clip < 0 || clip >= static_cast<int>(clips.size()))
        return;
    if (seconds <= 0.0f || m_Playing.empty())
    {
        Play(clip, loop);
        return;
    }

    m_Playing.push_back(ClipState{ clip, 0.0f, loop, 0.0f, seconds, 0.0f });
    playing = true;
}

int Animator::CurrentClip() const
{
    return m_Playing.empty() ? -1 : m_Playing.back().clip;
}

const std::shared_ptr<AnimationClip>& Animator::GetCurrentClip() const
{
    static const std::shared_ptr<AnimationClip> none;
    const int clip = CurrentClip();
    return clip >= 0 && clip < static_cast<int>(clips.size()) ? clips[clip].clip.Get() : none;
}

float Animator::Time() const
{
    return m_Playing.empty() ? 0.0f : m_Playing.back().time;
}

void Animator::SetTime(float time)
{
    if (!m_Playing.empty())
        m_Playing.back().time = time;
}

bool Animator::Loop() const
{
    return m_Playing.empty() || m_Playing.back().loop;
}

void Animator::SetLoop(bool loop)
{
    if (!m_Playing.empty())
        m_Playing.back().loop = loop;
}

void Animator::EnsureParameters() const
{
    const AnimationGraph* definition = graph.Get().get();
    if (definition == m_ParameterGraph && (!definition || definition->Revision() == m_ParameterRevision))
        return;

    // A graph edited in place keeps the values of the parameters it still has (by name).
    std::vector<float> values;
    std::vector<glm::vec3> vectors;
    std::vector<std::string> names;
    if (definition)
    {
        for (const AnimationParameter& parameter : definition->parameters)
        {
            float value = parameter.defaultValue;
            glm::vec3 vector = parameter.defaultVector;
            if (definition == m_ParameterGraph)
            {
                for (size_t i = 0; i < m_ParameterNames.size() && i < m_Parameters.size(); ++i)
                {
                    if (m_ParameterNames[i] != parameter.name)
                        continue;
                    value = m_Parameters[i];
                    if (i < m_Vectors.size())
                        vector = m_Vectors[i];
                }
            }
            values.push_back(value);
            vectors.push_back(vector);
            names.push_back(parameter.name);
        }
    }
    m_ParameterGraph = definition;
    m_ParameterRevision = definition ? definition->Revision() : 0;
    m_Parameters = std::move(values);
    m_Vectors = std::move(vectors);
    m_ParameterNames = std::move(names);
}

void Animator::SetFloat(std::string_view name, float value)
{
    EnsureParameters();
    SetParameter(m_ParameterGraph ? m_ParameterGraph->FindParameter(name) : -1, value);
}

void Animator::SetBool(std::string_view name, bool value)
{
    SetFloat(name, value ? 1.0f : 0.0f);
}

void Animator::SetTrigger(std::string_view name)
{
    SetFloat(name, 1.0f);
}

void Animator::ResetTrigger(std::string_view name)
{
    SetFloat(name, 0.0f);
}

float Animator::GetFloat(std::string_view name) const
{
    EnsureParameters();
    return GetParameter(m_ParameterGraph ? m_ParameterGraph->FindParameter(name) : -1);
}

bool Animator::GetBool(std::string_view name) const
{
    return GetFloat(name) != 0.0f;
}

float Animator::GetParameter(int index) const
{
    EnsureParameters();
    return index >= 0 && index < static_cast<int>(m_Parameters.size()) ? m_Parameters[index] : 0.0f;
}

void Animator::SetParameter(int index, float value)
{
    EnsureParameters();
    if (index >= 0 && index < static_cast<int>(m_Parameters.size()))
        m_Parameters[index] = value;
}

void Animator::SetVector(std::string_view name, const glm::vec3& value)
{
    EnsureParameters();
    SetVectorParameter(m_ParameterGraph ? m_ParameterGraph->FindParameter(name) : -1, value);
}

glm::vec3 Animator::GetVector(std::string_view name) const
{
    EnsureParameters();
    return GetVectorParameter(m_ParameterGraph ? m_ParameterGraph->FindParameter(name) : -1);
}

glm::vec3 Animator::GetVectorParameter(int index) const
{
    EnsureParameters();
    return index >= 0 && index < static_cast<int>(m_Vectors.size()) ? m_Vectors[index] : glm::vec3(0.0f);
}

void Animator::SetVectorParameter(int index, const glm::vec3& value)
{
    EnsureParameters();
    if (index >= 0 && index < static_cast<int>(m_Vectors.size()))
        m_Vectors[index] = value;
}

size_t Animator::BoundTracks() const
{
    const int clip = CurrentClip();
    return clip >= 0 && clip < static_cast<int>(m_Bindings.size()) ? m_Bindings[clip].bound : 0;
}

bool Animator::NeedsBind() const
{
    if (!m_Bound || m_BoundSkeleton != skeleton.Get().get() || m_BoundGraph != graph.Get().get() || static_cast<int>(m_BoundClips.size()) != ClipCount())
        return true;
    if (rootBone != m_BoundRootBone)
        return true;
    if (graph.Get() && graph.Get()->Revision() != m_BoundRevision)
        return true;
    for (int i = 0; i < ClipCount(); ++i)
        if (m_BoundClips[i] != GetClip(i)->clip.Get().get())
            return true;
    return false;
}

void Animator::Bind(Entity root)
{
    const Skeleton& bones = *skeleton.Get();
    m_BoundSkeleton = &bones;

    m_BoundClips.clear();
    m_Bindings.clear();
    m_Channels.assign(bones.BoneCount(), 0);
    for (int index = 0; index < ClipCount(); ++index)
    {
        const AnimationClip* clip = GetClip(index)->clip.Get().get();
        m_BoundClips.push_back(clip);
        m_Bindings.push_back(clip ? ClipBinding::Bind(*clip, bones) : ClipBinding{});
        if (!clip)
            continue;

        const ClipBinding& binding = m_Bindings.back();
        for (size_t i = 0; i < clip->tracks.size(); ++i)
        {
            const int bone = binding.bones[i];
            if (bone < 0)
                continue;
            const AnimationTrack& track = clip->tracks[i];
            m_Channels[bone] |= (track.translation.Empty() ? 0 : kTranslation)
                              | (track.rotation.Empty() ? 0 : kRotation)
                              | (track.scale.Empty() ? 0 : kScale);
        }
    }

    ResolveRootBone();

    // The bones' entities, found once here instead of every frame.
    m_Bones.assign(bones.BoneCount(), Entity{});
    m_BoundBones = 0;
    auto visit = [&](auto& self, Entity e, bool isRoot) -> void {
        if (!isRoot && (e.Has<Animator>() || !e.Has<AnimationTargetEntity>()))
            return;
        if (const AnimationTargetEntity* target = e.TryGet<AnimationTargetEntity>())
        {
            const int bone = bones.FindBoneByNode(target->nodeIndex);
            if (bone >= 0 && !m_Bones[bone].IsValid())
            {
                m_Bones[bone] = e;
                ++m_BoundBones;
            }
        }
        e.ForEachChild([&](Entity child) { self(self, child, false); });
    };
    visit(visit, root, true);

    const AnimationGraph* definition = graph.Get().get();
    m_GraphError.clear();
    m_GraphValid = definition && definition->Validate(m_GraphError);
    m_BoundRevision = definition ? definition->Revision() : 0;
    if (m_GraphValid)
        BindGraph(*definition);
    else
    {
        m_Nodes.clear();
        m_SyncGroupOf.clear();
        m_SyncGroups.clear();
        m_Masks.clear();
        m_References.clear();
        m_Machines.clear();
        m_StateNodes.clear();
        m_BlendWeights.clear();
    }
    m_BoundGraph = definition;

    m_Scratch.resize(std::max<size_t>(m_Nodes.size() + 1, 2));
    m_Pose.SetToRest(bones);
    m_Applied.clear();
    m_Bound = true;
}

void Animator::BindGraph(const AnimationGraph& definition)
{
    const size_t count = definition.nodes.size();
    // Rebinding the same graph (the hierarchy changed) keeps the clips' times.
    if (m_BoundGraph != &definition || m_Nodes.size() != count)
    {
        m_Nodes.assign(count, NodeState{});
        m_FreshNodes.assign(count, 1);
        m_SyncGroups.clear();
        m_Machines.assign(count, MachineState{});
    }
    m_StateNodes.assign(count, {});
    for (size_t i = 0; i < count; ++i)
    {
        std::vector<ActiveState>& active = m_Machines[i].active;
        const AnimationNode& node = definition.nodes[i];
        if (node.type != AnimationNodeType::StateMachine)
            active.clear();
        else
            active.erase(std::remove_if(active.begin(), active.end(), [&](const ActiveState& a) { return a.state >= static_cast<int>(node.states.size()); }), active.end());
    }
    for (SyncGroup& group : m_SyncGroups)
        group.members.clear();
    m_SyncGroupOf.assign(count, -1);
    m_IK.assign(count, {});
    m_HasIK = false;
    m_Masks.assign(count, {});
    m_References.assign(count, {});
    m_BlendWeights.assign(count, {});

    for (size_t i = 0; i < count; ++i)
    {
        const AnimationNode& node = definition.nodes[i];
        if (node.type == AnimationNodeType::Clip)
        {
            m_Nodes[i].clip = FindClip(node.clip);
            if (m_Nodes[i].clip < 0)
                Logger::Warning("Animator: the graph's node " + node.name + " plays " + node.clip + ", which isn't one of its clips");

            if (!node.syncGroup.empty())
            {
                auto group = std::find_if(m_SyncGroups.begin(), m_SyncGroups.end(), [&](const SyncGroup& g) { return g.name == node.syncGroup; });
                if (group == m_SyncGroups.end())
                    group = m_SyncGroups.insert(m_SyncGroups.end(), SyncGroup{ node.syncGroup });
                group->members.push_back(static_cast<int>(i));
                m_SyncGroupOf[i] = static_cast<int>(group - m_SyncGroups.begin());
            }
        }
        if (node.type == AnimationNodeType::Layer)
            m_Masks[i] = node.mask.Resolve(*m_BoundSkeleton);
        if (node.type == AnimationNodeType::TwoBoneIK || node.type == AnimationNodeType::LookAt)
        {
            // The bones it moves, top first: a limb's upper, middle and end bone, or a look's chain.
            const Skeleton& bones = *m_BoundSkeleton;
            const int end = bones.FindBone(node.bone);
            const int length = node.type == AnimationNodeType::TwoBoneIK ? 3 : std::max(node.chainLength, 1);
            std::vector<int> chain;
            for (int b = end; b >= 0 && static_cast<int>(chain.size()) < length; b = bones.GetBone(b).parent)
                chain.insert(chain.begin(), b);
            if (end < 0 || (node.type == AnimationNodeType::TwoBoneIK && chain.size() < 3))
                Logger::Warning("Animator: the graph's node " + node.name + " moves " + node.bone + ", which the skeleton doesn't have" +
                                (end >= 0 ? " two bones above" : ""));
            else
            {
                m_IK[i].bones = std::move(chain);
                // Where the bone faces: the model's +Z, as it rests.
                glm::quat rest(1.0f, 0.0f, 0.0f, 0.0f);
                for (int b = end; b >= 0; b = bones.GetBone(b).parent)
                    rest = bones.GetBone(b).rest.rotation * rest;
                m_IK[i].forward = glm::normalize(glm::inverse(glm::normalize(rest)) * glm::vec3(0.0f, 0.0f, 1.0f));
                m_HasIK = true;
                // The bones it turns are written like the ones clips animate.
                for (int b : m_IK[i].bones)
                    m_Channels[b] |= kRotation;
            }
        }
        if (node.type == AnimationNodeType::Blend2D)
            m_BlendWeights[i].assign(node.inputs.size(), 0.0f);

        if (node.type == AnimationNodeType::StateMachine)
        {
            // What entering each state restarts.
            m_StateNodes[i].resize(node.inputs.size());
            for (size_t state = 0; state < node.inputs.size(); ++state)
            {
                std::vector<bool> seen(count, false);
                std::vector<int>& below = m_StateNodes[i][state];
                auto collect = [&](auto& self, int n) -> void {
                    if (seen[n])
                        return;
                    seen[n] = true;
                    below.push_back(n);
                    for (int input : definition.nodes[n].inputs)
                        self(self, input);
                };
                collect(collect, node.inputs[state]);
            }
        }

        const bool additive = node.type == AnimationNodeType::Additive || (node.type == AnimationNodeType::Layer && node.layerMode == LayerMode::Additive);
        if (additive && node.inputs.size() == 2)
        {
            // What the additive input is relative to: its clip's first frame, or the rest pose.
            const AnimationNode& input = definition.nodes[node.inputs[1]];
            const int clip = input.type == AnimationNodeType::Clip ? FindClip(input.clip) : -1;
            SampleInto(node.reference == AdditiveReference::FirstFrame ? clip : -1, 0.0f, m_References[i]);
        }
    }
}

void Animator::SampleInto(int clip, float time, Pose& pose) const
{
    pose.SetToRest(*m_BoundSkeleton);
    if (clip >= 0 && clip < static_cast<int>(m_BoundClips.size()) && m_BoundClips[clip])
    {
        SampleClip(*m_BoundClips[clip], m_Bindings[clip], time, pose);
        if (rootMotion)
            StripRoot(clip, pose);
    }
}

void Animator::ResolveRootBone()
{
    const Skeleton& bones = *m_BoundSkeleton;
    m_BoundRootBone = rootBone;
    m_RootBone = -1;
    if (!rootBone.empty())
        m_RootBone = bones.FindBone(rootBone);
    else
    {
        // The hips (or pelvis) when a clip moves or turns them, else the first bone a clip moves.
        for (size_t b = 0; b < bones.BoneCount() && m_RootBone < 0; ++b)
        {
            if (!m_Channels[b])
                continue;
            std::string name(Skeleton::BaseName(bones.GetBone(b).name));
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (name.find("hips") != std::string::npos || name.find("pelvis") != std::string::npos)
                m_RootBone = static_cast<int>(b);
        }
        for (size_t b = 0; b < bones.BoneCount() && m_RootBone < 0; ++b)
            if (m_Channels[b] & kTranslation)
                m_RootBone = static_cast<int>(b);
    }

    m_RootTracks.assign(m_BoundClips.size(), -1);
    m_RootParent = glm::mat4(1.0f);
    if (m_RootBone < 0)
        return;
    // The bones above it as they rest (an Armature node's scale and turn, say).
    std::vector<int> chain;
    for (int b = bones.GetBone(m_RootBone).parent; b >= 0; b = bones.GetBone(b).parent)
        chain.push_back(b);
    for (auto it = chain.rbegin(); it != chain.rend(); ++it)
        m_RootParent = m_RootParent * Local(bones.GetBone(*it).rest);
    m_RootParentInverse = glm::inverse(m_RootParent);
    const glm::mat3 axes(m_RootParent);
    m_RootParentRotation = glm::normalize(glm::quat_cast(glm::mat3(glm::normalize(axes[0]), glm::normalize(axes[1]), glm::normalize(axes[2]))));
    const BoneTransform& rest = bones.GetBone(m_RootBone).rest;
    m_RootRestPosition = glm::vec3(m_RootParent * glm::vec4(rest.translation, 1.0f));
    m_RootRestRotation = m_RootParentRotation * rest.rotation;
    for (size_t clip = 0; clip < m_BoundClips.size(); ++clip)
    {
        if (!m_BoundClips[clip])
            continue;
        const std::vector<int>& tracks = m_Bindings[clip].bones;
        for (size_t t = 0; t < tracks.size(); ++t)
            if (tracks[t] == m_RootBone)
                m_RootTracks[clip] = static_cast<int>(t);
    }
}

bool Animator::RootAt(int clip, float time, glm::vec3& position, float& yaw) const
{
    if (m_RootBone < 0 || clip < 0 || clip >= static_cast<int>(m_RootTracks.size()) || m_RootTracks[clip] < 0)
        return false;
    const AnimationTrack& track = m_BoundClips[clip]->tracks[m_RootTracks[clip]];
    const BoneTransform& rest = m_BoundSkeleton->GetBone(m_RootBone).rest;
    const glm::vec3 local = track.translation.Empty() ? rest.translation : glm::vec3(track.translation.Sample(time, false));
    glm::quat rotation = rest.rotation;
    if (!track.rotation.Empty())
    {
        const glm::vec4 q = track.rotation.Sample(time, true);
        rotation = glm::quat(q.w, q.x, q.y, q.z);
    }
    position = glm::vec3(m_RootParent * glm::vec4(local, 1.0f));
    yaw = YawOf(m_RootParentRotation * rotation * glm::inverse(m_RootRestRotation));
    return true;
}

void Animator::StripRoot(int clip, Pose& pose) const
{
    if (m_RootBone < 0 || clip >= static_cast<int>(m_RootTracks.size()) || m_RootTracks[clip] < 0)
        return;
    // Where the clip has the root, in the model's space: back over its rest spot on the ground...
    glm::vec3 position = glm::vec3(m_RootParent * glm::vec4(pose.translations[m_RootBone], 1.0f));
    position.x = m_RootRestPosition.x;
    position.z = m_RootRestPosition.z;
    pose.translations[m_RootBone] = glm::vec3(m_RootParentInverse * glm::vec4(position, 1.0f));
    // ...and turned back to face the way it rests.
    const glm::quat model = m_RootParentRotation * pose.rotations[m_RootBone];
    const float yaw = YawOf(model * glm::inverse(m_RootRestRotation));
    const glm::quat untwist = glm::angleAxis(-yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    pose.rotations[m_RootBone] = glm::normalize(glm::inverse(m_RootParentRotation) * untwist * model);
}

void Animator::AddRootMotion(int clip, float from, float to, int wraps, float weight)
{
    glm::vec3 a, b;
    float yawA, yawB;
    if (weight <= 0.0f || !RootAt(clip, from, a, yawA) || !RootAt(clip, to, b, yawB))
        return;
    glm::vec3 move = b - a;
    float turn = WrapAngle(yawB - yawA);
    if (wraps != 0)
    {
        // Each loop adds what one whole play of the clip moves.
        glm::vec3 start, end;
        float yawStart, yawEnd;
        RootAt(clip, 0.0f, start, yawStart);
        RootAt(clip, m_BoundClips[clip]->GetDuration(), end, yawEnd);
        move += static_cast<float>(wraps) * (end - start);
        turn += static_cast<float>(wraps) * WrapAngle(yawEnd - yawStart);
    }
    // The clip's move in the entity's own frame: the clip had turned by yawA when it made it.
    move.y = 0.0f;
    m_RootDelta += weight * (glm::angleAxis(-yawA, glm::vec3(0.0f, 1.0f, 0.0f)) * move);
    m_RootYaw += weight * turn;
}

glm::mat4 Animator::ModelMatrix(const Pose& pose, int bone) const
{
    glm::mat4 m(1.0f);
    for (int b = bone; b >= 0; b = m_BoundSkeleton->GetBone(b).parent)
        m = Local(BoneTransform{ pose.translations[b], pose.rotations[b], pose.scales[b] }) * m;
    return m;
}

glm::quat Animator::ModelRotation(const Pose& pose, int bone) const
{
    glm::quat q(1.0f, 0.0f, 0.0f, 0.0f);
    for (int b = bone; b >= 0; b = m_BoundSkeleton->GetBone(b).parent)
        q = pose.rotations[b] * q;
    return glm::normalize(q);
}

namespace
{
    // The turn taking direction `from` onto `to` (both unit length), the shortest way.
    glm::quat Between(const glm::vec3& from, const glm::vec3& to)
    {
        const float d = glm::clamp(glm::dot(from, to), -1.0f, 1.0f);
        if (d > 0.999999f)
            return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 axis = glm::cross(from, to);
        if (glm::length(axis) < 1e-6f)
        {
            axis = glm::cross(from, glm::vec3(1.0f, 0.0f, 0.0f));
            if (glm::length(axis) < 1e-6f)
                axis = glm::cross(from, glm::vec3(0.0f, 1.0f, 0.0f));
        }
        return glm::angleAxis(std::acos(d), glm::normalize(axis));
    }

    // Turns bone `bone` by `turn` given in the model's space (about its own joint), keeping its parent.
    void TurnInModel(Pose& pose, int bone, const glm::quat& parentModel, const glm::quat& turn)
    {
        const glm::quat model = parentModel * pose.rotations[bone];
        pose.rotations[bone] = glm::normalize(glm::inverse(parentModel) * (turn * model));
    }
}

void Animator::SolveTwoBoneIK(int index, Pose& pose) const
{
    const IKChain& chain = m_IK[index];
    const AnimationNode& node = m_BoundGraph->nodes[index];
    const float weight = WeightOf(node.weight);
    if (chain.bones.size() != 3 || weight <= 0.0f)
        return;
    const int upper = chain.bones[0], middle = chain.bones[1], end = chain.bones[2];
    const glm::quat upperBefore = pose.rotations[upper], middleBefore = pose.rotations[middle];
    const int above = m_BoundSkeleton->GetBone(upper).parent;
    const glm::quat aboveModel = above >= 0 ? ModelRotation(pose, above) : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    const glm::vec3 target = glm::vec3(m_ModelFromWorld * glm::vec4(GetVectorParameter(node.parameter), 1.0f));

    glm::vec3 a = glm::vec3(ModelMatrix(pose, upper)[3]);
    glm::vec3 b = glm::vec3(ModelMatrix(pose, middle)[3]);
    glm::vec3 c = glm::vec3(ModelMatrix(pose, end)[3]);
    const float upperLength = glm::length(b - a), lowerLength = glm::length(c - b);
    if (upperLength < 1e-6f || lowerLength < 1e-6f)
        return;
    // As far as the limb reaches (and no nearer than it folds).
    const float reach = glm::clamp(glm::length(target - a), std::abs(upperLength - lowerLength) + 1e-4f, (upperLength + lowerLength) * 0.9999f);
    const bool hasPole = node.parameterY >= 0;
    const glm::vec3 pole = hasPole ? glm::vec3(m_ModelFromWorld * glm::vec4(GetVectorParameter(node.parameterY), 1.0f)) : glm::vec3(0.0f);

    // The middle joint first, about the normal of the plane the limb bends in (the pose's; a
    // straight limb takes the pole's, else any): open or close it to the angle that makes the reach.
    glm::vec3 bend = glm::cross(b - a, c - b);
    if (glm::length(bend) < 1e-6f && hasPole)
        bend = glm::cross(target - a, pole - a);
    if (glm::length(bend) < 1e-6f)
        bend = glm::cross(c - a, glm::vec3(0.0f, 0.0f, 1.0f));
    if (glm::length(bend) < 1e-6f)
        bend = glm::cross(c - a, glm::vec3(1.0f, 0.0f, 0.0f));
    const glm::vec3 axis = glm::normalize(bend);
    const auto angle = [](const glm::vec3& u, const glm::vec3& v) { return std::acos(glm::clamp(glm::dot(glm::normalize(u), glm::normalize(v)), -1.0f, 1.0f)); };
    const float middleNow = angle(a - b, c - b);
    const float middleWanted = std::acos(glm::clamp((upperLength * upperLength + lowerLength * lowerLength - reach * reach) / (2.0f * upperLength * lowerLength), -1.0f, 1.0f));
    // Which way about the axis opens it: try one, else the other.
    glm::quat open = glm::angleAxis(middleWanted - middleNow, axis);
    if (std::abs(angle(a - b, open * (c - b)) - middleWanted) > 1e-3f)
        open = glm::angleAxis(middleNow - middleWanted, axis);
    TurnInModel(pose, middle, ModelRotation(pose, upper), open);

    // Then the upper joint: swing the limb's end onto the target...
    c = glm::vec3(ModelMatrix(pose, end)[3]);
    glm::quat swing = Between(glm::normalize(c - a), glm::normalize(target - a));
    // ...and about the reach, so the middle joint goes the pole's way.
    if (hasPole)
    {
        const glm::vec3 along = glm::normalize(target - a);
        glm::vec3 middleDir = swing * (b - a);
        middleDir -= along * glm::dot(middleDir, along);
        glm::vec3 poleDir = pole - a;
        poleDir -= along * glm::dot(poleDir, along);
        if (glm::length(middleDir) > 1e-6f && glm::length(poleDir) > 1e-6f)
            swing = Between(glm::normalize(middleDir), glm::normalize(poleDir)) * swing;
    }
    TurnInModel(pose, upper, aboveModel, swing);

    if (weight < 1.0f)
    {
        pose.rotations[upper] = glm::slerp(upperBefore, pose.rotations[upper], weight);
        pose.rotations[middle] = glm::slerp(middleBefore, pose.rotations[middle], weight);
    }
}

void Animator::SolveLookAt(int index, Pose& pose) const
{
    const IKChain& chain = m_IK[index];
    const AnimationNode& node = m_BoundGraph->nodes[index];
    const float weight = WeightOf(node.weight);
    if (chain.bones.empty() || weight <= 0.0f)
        return;
    const int head = chain.bones.back();
    std::vector<glm::quat> before;
    for (int bone : chain.bones)
        before.push_back(pose.rotations[bone]);
    const glm::vec3 target = glm::vec3(m_ModelFromWorld * glm::vec4(GetVectorParameter(node.parameter), 1.0f));

    // Where it should look from where its eye is: toward the target, at most the angle limit away
    // from where the pose has it facing.
    const glm::vec3 facing = glm::normalize(ModelRotation(pose, head) * chain.forward);
    const float limit = glm::radians(std::clamp(node.angleLimit, 0.0f, 180.0f));
    const auto wanted = [&]() {
        const glm::vec3 eye = glm::vec3(ModelMatrix(pose, head)[3]);
        if (glm::length(target - eye) < 1e-4f)
            return facing;
        const glm::vec3 to = glm::normalize(target - eye);
        const float apart = std::acos(glm::clamp(glm::dot(facing, to), -1.0f, 1.0f));
        return apart <= limit ? to : glm::normalize(glm::slerp(glm::quat(1.0f, 0.0f, 0.0f, 0.0f), Between(facing, to), limit / apart) * facing);
    };

    // Each bone of the chain, from the top, turns its share of what is left; the last one finishes.
    const size_t count = chain.bones.size();
    for (size_t k = 0; k < count; ++k)
    {
        const int bone = chain.bones[k];
        const glm::vec3 now = glm::normalize(ModelRotation(pose, head) * chain.forward);
        const glm::quat left = Between(now, wanted());
        const glm::quat share = glm::slerp(glm::quat(1.0f, 0.0f, 0.0f, 0.0f), left, 1.0f / static_cast<float>(count - k));
        const int parent = m_BoundSkeleton->GetBone(bone).parent;
        TurnInModel(pose, bone, parent >= 0 ? ModelRotation(pose, parent) : glm::quat(1.0f, 0.0f, 0.0f, 0.0f), share);
    }

    if (weight < 1.0f)
        for (size_t k = 0; k < count; ++k)
            pose.rotations[chain.bones[k]] = glm::slerp(before[k], pose.rotations[chain.bones[k]], weight);
}

void Animator::FireEvents(int clip, float from, float to, float step, bool loop, bool fresh, float weight)
{
    const NamedClip* named = GetClip(clip);
    if (weight < kEventWeight || step == 0.0f || !named || named->events.empty() || clip >= static_cast<int>(m_BoundClips.size()) || !m_BoundClips[clip])
        return;
    const float duration = m_BoundClips[clip]->GetDuration();
    const int wraps = Wraps(from, to, step, loop);
    const auto crossed = [&](float t) {
        if (step > 0.0f)
            return wraps == 0 ? (fresh ? t >= from : t > from) && t <= to : (t > from && t <= duration) || (t >= 0.0f && t <= to);
        return wraps == 0 ? (fresh ? t <= from : t < from) && t >= to : (t < from && t >= 0.0f) || (t <= duration && t >= to);
    };
    for (const AnimationEvent& event : named->events)
    {
        if (!crossed(event.time))
            continue;
        auto same = std::find_if(m_Events.begin(), m_Events.end(), [&](const FiredEvent& e) { return e.name == event.name; });
        if (same == m_Events.end())
            m_Events.push_back({ event.name, named->name, weight });
        else if (weight > same->weight)
            *same = { event.name, named->name, weight };
    }
}

void Animator::ApplyRootMotion(Entity root, float dt)
{
    const glm::vec3 move = m_RootDelta;
    const float turn = m_RootYaw;
    m_RootDelta = glm::vec3(0.0f);
    m_RootYaw = 0.0f;
    m_RootVelocity = dt > 0.0f ? move / dt : glm::vec3(0.0f);
    m_RootTurnRate = dt > 0.0f ? turn / dt : 0.0f;
    if (!rootMotion || dt <= 0.0f)
        return;

    // Something else moves the entity (a physics character): it gets the motion.
    if (RootMotion* sink = root.Get<RootMotion>())
    {
        sink->translation += move;
        sink->yaw += turn;
        sink->seconds += dt;
        return;
    }
    if (move == glm::vec3(0.0f) && turn == 0.0f)
        return;
    LocalTRS* trs = root.Get<LocalTRS>();
    if (!trs)
        return;
    const glm::quat rotation(trs->rot_quat[3], trs->rot_quat[0], trs->rot_quat[1], trs->rot_quat[2]);
    const glm::vec3 step = rotation * (glm::vec3(trs->scl[0], trs->scl[1], trs->scl[2]) * move);
    trs->pos[0] += step.x;
    trs->pos[1] += step.y;
    trs->pos[2] += step.z;
    const glm::quat turned = glm::normalize(rotation * glm::angleAxis(turn, glm::vec3(0.0f, 1.0f, 0.0f)));
    trs->rot_quat[0] = turned.x;
    trs->rot_quat[1] = turned.y;
    trs->rot_quat[2] = turned.z;
    trs->rot_quat[3] = turned.w;
    ++trs->version;
}

Pose& Animator::Scratch(size_t depth)
{
    // Sized by Bind: evaluation goes at most one level deeper per node.
    return m_Scratch[std::min(depth, m_Scratch.size() - 1)];
}

void Animator::Update(Entity root, float dt)
{
    m_Events.clear();
    if (!skeleton.Get())
        return;
    if (!graph.Get() && m_Playing.empty())
        return;

    // Just bound (the first update, or after a change): the clips start, but whatever moves the
    // entity (root motion) waits an update, so a physics character can take it from the start.
    const bool justBound = NeedsBind();
    if (justBound)
        Bind(root);
    EnsureParameters();
    const bool useGraph = m_BoundGraph && m_GraphValid;
    if (!useGraph && m_Playing.empty())
        return;

    const float delta = playing ? dt * speed : 0.0f;
    if (useGraph)
        AdvanceGraph(delta);
    else
        AdvanceClips(delta);
    ApplyRootMotion(root, playing && !justBound ? dt : 0.0f);

    Profiler::Count("Animators", 1.0);
    // IK targets are world positions: where the entity is now takes them into the model's space.
    if (useGraph && m_HasIK && root.GetScene())
        m_ModelFromWorld = glm::inverse(WorldMatrix(*root.GetScene(), root));
    // Paused or held at the end: the pose is already written (unless something else moved the bones).
    if (!StateChanged(useGraph) && !m_Refresh)
        return;
    m_Refresh = false;
    Profiler::Count("Animators posed", 1.0);

    if (useGraph)
        EvaluateNode(m_BoundGraph->output, m_Pose, 0);
    else
        EvaluateClips();
    WriteBones();
}

void Animator::AdvanceClips(float delta)
{
    m_OldTimes.resize(m_Playing.size());
    for (size_t i = 0; i < m_Playing.size(); ++i)
    {
        ClipState& state = m_Playing[i];
        m_OldTimes[i] = state.time;
        const AnimationClip* clip = state.clip < static_cast<int>(m_BoundClips.size()) ? m_BoundClips[state.clip] : nullptr;
        state.time = Advance(state.time, delta, clip ? clip->GetDuration() : 0.0f, state.loop);
    }

    FadeWeights(m_Playing, delta);
    if (rootMotion && m_RootBone >= 0 && delta != 0.0f)
        for (size_t i = 0; i < m_Playing.size(); ++i)
        {
            const ClipState& state = m_Playing[i];
            AddRootMotion(state.clip, m_OldTimes[i], state.time, Wraps(m_OldTimes[i], state.time, delta, state.loop), state.weight);
        }
    if (delta != 0.0f)
        for (size_t i = 0; i < m_Playing.size(); ++i)
        {
            ClipState& state = m_Playing[i];
            FireEvents(state.clip, m_OldTimes[i], state.time, delta, state.loop, state.fresh, state.weight);
            state.fresh = false;
        }
    DropFaded(m_Playing);
}

void Animator::EvaluateClips()
{
    SampleInto(m_Playing[0].clip, m_Playing[0].time, m_Pose);
    for (size_t i = 1; i < m_Playing.size(); ++i)
    {
        Pose& other = Scratch(0);
        SampleInto(m_Playing[i].clip, m_Playing[i].time, other);
        BlendPoses(m_Pose, other, m_Playing[i].fade, m_Pose);
    }
}

float Animator::WeightOf(const AnimationWeight& weight) const
{
    const bool fromParameter = weight.parameter >= 0 && weight.parameter < static_cast<int>(m_Parameters.size());
    return std::clamp(fromParameter ? m_Parameters[weight.parameter] : weight.value, 0.0f, 1.0f);
}

void Animator::Segment(const AnimationNode& node, int& a, int& b, float& t) const
{
    const float value = GetParameter(node.parameter);
    const std::vector<float>& thresholds = node.thresholds;
    const size_t last = thresholds.size() - 1;
    t = 0.0f;
    if (value <= thresholds.front())
    {
        a = b = node.inputs.front();
        return;
    }
    if (value >= thresholds.back())
    {
        a = b = node.inputs[last];
        return;
    }
    size_t k = 0;
    while (k + 1 < last && thresholds[k + 1] <= value)
        ++k;
    const float span = thresholds[k + 1] - thresholds[k];
    t = span > 0.0f ? (value - thresholds[k]) / span : 0.0f;
    a = node.inputs[k];
    b = node.inputs[k + 1];
}

void Animator::Weigh(int index, float weight, float motion)
{
    if (weight <= 0.0f)
        return;
    m_Nodes[index].weight += weight;
    m_MotionWeights[index] += motion;

    const AnimationNode& node = m_BoundGraph->nodes[index];
    switch (node.type)
    {
    case AnimationNodeType::Clip:
        break;
    case AnimationNodeType::Blend:
    {
        const float x = WeightOf(node.weight);
        Weigh(node.inputs[0], weight * (1.0f - x), motion * (1.0f - x));
        Weigh(node.inputs[1], weight * x, motion * x);
        break;
    }
    case AnimationNodeType::Blend1D:
    {
        int a, b;
        float t;
        Segment(node, a, b, t);
        Weigh(a, weight * (1.0f - t), motion * (1.0f - t));
        if (b != a)
            Weigh(b, weight * t, motion * t);
        break;
    }
    case AnimationNodeType::Additive:
    case AnimationNodeType::Layer:
        // A layer (a wave on top of a walk) doesn't walk.
        Weigh(node.inputs[0], weight, motion);
        Weigh(node.inputs[1], weight * WeightOf(node.weight), 0.0f);
        break;
    case AnimationNodeType::Blend2D:
        Weights2D(index);
        for (size_t i = 0; i < node.inputs.size(); ++i)
            Weigh(node.inputs[i], weight * m_BlendWeights[index][i], motion * m_BlendWeights[index][i]);
        break;
    case AnimationNodeType::StateMachine:
        for (const ActiveState& active : m_Machines[index].active)
            Weigh(node.inputs[active.state], weight * active.weight, motion * active.weight);
        break;
    case AnimationNodeType::TwoBoneIK:
    case AnimationNodeType::LookAt:
        Weigh(node.inputs[0], weight, motion);
        break;
    }
}

void Animator::AdvanceGraph(float delta)
{
    const AnimationGraph& definition = *m_BoundGraph;
    if (!m_PendingStates.empty())
    {
        const std::vector<PendingState> pending = std::move(m_PendingStates);
        m_PendingStates.clear();
        for (const PendingState& request : pending)
        {
            if (request.normalizedTime >= 0.0f)
                PreviewState(request.state, request.normalizedTime);
            else
                CrossFadeState(request.state, request.seconds);
        }
    }
    // A preview holds until the game moves.
    if (delta != 0.0f)
        m_Preview = false;
    for (size_t i = 0; i < definition.nodes.size(); ++i)
        if (definition.nodes[i].type == AnimationNodeType::StateMachine)
            UpdateMachine(static_cast<int>(i), delta);

    for (NodeState& state : m_Nodes)
        state.weight = 0.0f;
    m_MotionWeights.assign(m_Nodes.size(), 0.0f);
    Weigh(definition.output, 1.0f, 1.0f);
    // Where the clips were before this update (entering a state already put its clips at 0).
    m_OldTimes.resize(m_Nodes.size());
    for (size_t i = 0; i < m_Nodes.size(); ++i)
        m_OldTimes[i] = m_Nodes[i].time;

    auto duration = [&](int clip) {
        return clip >= 0 && clip < static_cast<int>(m_BoundClips.size()) && m_BoundClips[clip] ? m_BoundClips[clip]->GetDuration() : 0.0f;
    };

    for (size_t i = 0; i < definition.nodes.size(); ++i)
    {
        const AnimationNode& node = definition.nodes[i];
        if (node.type != AnimationNodeType::Clip || m_SyncGroupOf[i] >= 0)
            continue;
        NodeState& state = m_Nodes[i];
        state.time = Advance(state.time, delta * node.speed, duration(state.clip), node.loop);
    }

    // A sync group moves through its clips' normalized time at the rate their weights blend to.
    for (SyncGroup& group : m_SyncGroups)
    {
        float weighted = 0.0f, total = 0.0f, unweighted = 0.0f;
        int count = 0;
        bool loop = false;
        for (int member : group.members)
        {
            const float length = duration(m_Nodes[member].clip);
            if (length <= 0.0f)
                continue;
            const float rate = definition.nodes[member].speed / length;
            weighted += m_Nodes[member].weight * rate;
            total += m_Nodes[member].weight;
            unweighted += rate;
            loop |= definition.nodes[member].loop;
            ++count;
        }
        if (count == 0)
            continue;

        const float rate = total > 0.0f ? weighted / total : unweighted / count;
        group.phase = Advance(group.phase, delta * rate, 1.0f, loop);
        for (int member : group.members)
            m_Nodes[member].time = group.phase * duration(m_Nodes[member].clip);
    }

    // Events: each clip node's, by its share of the pose.
    if (delta != 0.0f)
    {
        m_FreshNodes.resize(m_Nodes.size(), 1);
        for (size_t i = 0; i < definition.nodes.size(); ++i)
        {
            const AnimationNode& node = definition.nodes[i];
            if (node.type != AnimationNodeType::Clip || m_Nodes[i].weight <= 0.0f)
                continue;
            const float step = m_SyncGroupOf[i] >= 0 ? delta : delta * node.speed;
            FireEvents(m_Nodes[i].clip, m_OldTimes[i], m_Nodes[i].time, step, node.loop, m_FreshNodes[i] != 0, m_Nodes[i].weight);
            m_FreshNodes[i] = 0;
        }
    }

    // The root's move: each clip's, by its share of the motion.
    if (rootMotion && m_RootBone >= 0 && delta != 0.0f)
    {
        for (size_t i = 0; i < definition.nodes.size(); ++i)
        {
            const AnimationNode& node = definition.nodes[i];
            if (node.type != AnimationNodeType::Clip || m_MotionWeights[i] <= 0.0f)
                continue;
            const float step = m_SyncGroupOf[i] >= 0 ? delta : delta * node.speed;
            AddRootMotion(m_Nodes[i].clip, m_OldTimes[i], m_Nodes[i].time, Wraps(m_OldTimes[i], m_Nodes[i].time, step, node.loop), m_MotionWeights[i]);
        }
    }

    // How far each state got, for exit times: its clips' normalized rate, weighted like they are.
    for (size_t i = 0; i < m_Machines.size(); ++i)
    {
        for (ActiveState& active : m_Machines[i].active)
        {
            float weighted = 0.0f, total = 0.0f, unweighted = 0.0f;
            int count = 0;
            for (int n : m_StateNodes[i][active.state])
            {
                const float length = definition.nodes[n].type == AnimationNodeType::Clip ? duration(m_Nodes[n].clip) : 0.0f;
                if (length <= 0.0f)
                    continue;
                const float rate = definition.nodes[n].speed / length;
                weighted += m_Nodes[n].weight * rate;
                total += m_Nodes[n].weight;
                unweighted += rate;
                ++count;
            }
            if (count > 0)
                active.normalizedTime += delta * (total > 0.0f ? weighted / total : unweighted / count);
        }
    }
}

void Animator::UpdateMachine(int index, float delta)
{
    const AnimationNode& node = m_BoundGraph->nodes[index];
    MachineState& machine = m_Machines[index];
    if (machine.active.empty())
        StartState(index, node.entryState, 0.0f, true);

    // A running transition blocks the others unless it may be interrupted.
    const ActiveState& current = machine.active.back();
    if (!m_Preview && (current.fade >= 1.0f || current.interruptible))
    {
        for (const AnimationTransition& transition : node.transitions)
        {
            if (transition.from >= 0 ? transition.from != current.state : transition.to == current.state)
                continue;
            if (transition.exitTime >= 0.0f && current.normalizedTime < transition.exitTime)
                continue;
            if (!ConditionsHold(transition))
                continue;

            for (const AnimationCondition& condition : transition.conditions)
                if (m_BoundGraph->parameters[condition.parameter].type == AnimationParameterType::Trigger)
                    m_Parameters[condition.parameter] = 0.0f;
            StartState(index, transition.to, transition.duration, transition.interruptible);
            break;
        }
    }

    FadeIn(machine.active, delta);
}

bool Animator::ConditionsHold(const AnimationTransition& transition) const
{
    for (const AnimationCondition& condition : transition.conditions)
    {
        const float value = GetParameter(condition.parameter);
        bool holds = false;
        switch (condition.mode)
        {
        case ConditionMode::If:        holds = value != 0.0f; break;
        case ConditionMode::IfNot:     holds = value == 0.0f; break;
        case ConditionMode::Greater:   holds = value > condition.threshold; break;
        case ConditionMode::Less:      holds = value < condition.threshold; break;
        case ConditionMode::Equals:    holds = value == condition.threshold; break;
        case ConditionMode::NotEquals: holds = value != condition.threshold; break;
        }
        if (!holds)
            return false;
    }
    return true;
}

void Animator::StartState(int index, int state, float duration, bool interruptible)
{
    EnterState(index, state);
    MachineState& machine = m_Machines[index];
    ActiveState entry{ state, 0.0f, duration, 0.0f, 0.0f, interruptible };
    if (duration <= 0.0f || machine.active.empty())
    {
        entry.fade = entry.weight = 1.0f;
        machine.active.assign(1, entry);
    }
    else
    {
        machine.active.push_back(entry);
    }
}

void Animator::EnterState(int index, int state)
{
    // The state plays from its start: its clips, their sync groups and the state machines inside it.
    for (int n : m_StateNodes[index][state])
    {
        const AnimationNodeType type = m_BoundGraph->nodes[n].type;
        if (type == AnimationNodeType::Clip)
        {
            m_Nodes[n].time = 0.0f;
            if (static_cast<size_t>(n) < m_FreshNodes.size())
                m_FreshNodes[n] = 1; // its events at 0 fire as it starts
            if (m_SyncGroupOf[n] >= 0)
                m_SyncGroups[m_SyncGroupOf[n]].phase = 0.0f;
        }
        else if (type == AnimationNodeType::StateMachine && n != index)
        {
            StartState(n, m_BoundGraph->nodes[n].entryState, 0.0f, true);
        }
    }
}

bool Animator::CrossFadeState(std::string_view state, float seconds)
{
    const AnimationGraph* definition = graph.Get().get();
    if (!definition)
        return false;

    for (size_t i = 0; i < definition->nodes.size(); ++i)
    {
        const AnimationNode& node = definition->nodes[i];
        const int found = node.type == AnimationNodeType::StateMachine ? node.FindState(state) : -1;
        if (found < 0)
            continue;
        // Before the first update the graph isn't bound yet: it moves there then.
        m_Preview = false;
        if (!m_Bound || m_BoundGraph != definition || !m_GraphValid)
            m_PendingStates.push_back({ std::string(state), seconds });
        else
            StartState(static_cast<int>(i), found, seconds, true);
        return true;
    }
    return false;
}

std::vector<std::string> Animator::CurrentStates() const
{
    std::vector<std::string> states;
    const AnimationGraph* definition = graph.Get().get();
    if (!definition)
        return states;
    if (!m_Bound || m_BoundGraph != definition || !m_GraphValid)
    {
        for (const PendingState& pending : m_PendingStates)
            states.push_back(pending.state);
        return states;
    }
    for (size_t i = 0; i < m_Machines.size() && i < definition->nodes.size(); ++i)
    {
        const AnimationNode& node = definition->nodes[i];
        if (node.type != AnimationNodeType::StateMachine || m_Machines[i].active.empty())
            continue;
        const int state = m_Machines[i].active.back().state;
        if (state != node.entryState && state >= 0 && state < static_cast<int>(node.states.size()))
            states.push_back(node.states[static_cast<size_t>(state)]);
    }
    return states;
}

bool Animator::PreviewState(std::string_view state, float normalizedTime)
{
    const AnimationGraph* definition = graph.Get().get();
    if (!definition)
        return false;

    for (size_t i = 0; i < definition->nodes.size(); ++i)
    {
        const AnimationNode& node = definition->nodes[i];
        const int found = node.type == AnimationNodeType::StateMachine ? node.FindState(state) : -1;
        if (found < 0)
            continue;
        if (!m_Bound || m_BoundGraph != definition || !m_GraphValid)
        {
            m_PendingStates.push_back({ std::string(state), 0.0f, std::max(normalizedTime, 0.0f) });
            return true;
        }
        StartState(static_cast<int>(i), found, 0.0f, true);
        SetStateTime(static_cast<int>(i), found, std::max(normalizedTime, 0.0f));
        m_Preview = true;
        return true;
    }
    return false;
}

void Animator::SetStateTime(int index, int state, float normalizedTime)
{
    const AnimationGraph& definition = *m_BoundGraph;
    for (int n : m_StateNodes[index][state])
    {
        if (definition.nodes[n].type != AnimationNodeType::Clip)
            continue;
        const int clip = m_Nodes[n].clip;
        const float length = clip >= 0 && clip < static_cast<int>(m_BoundClips.size()) && m_BoundClips[clip] ? m_BoundClips[clip]->GetDuration() : 0.0f;
        m_Nodes[n].time = Advance(normalizedTime * length, 0.0f, length, definition.nodes[n].loop);
        if (m_SyncGroupOf[n] >= 0)
            m_SyncGroups[m_SyncGroupOf[n]].phase = Advance(normalizedTime, 0.0f, 1.0f, definition.nodes[n].loop);
    }
    m_Machines[index].active.back().normalizedTime = normalizedTime;
}

void Animator::Weights2D(int index)
{
    // Gradient band interpolation (freeform cartesian): each input's weight falls off toward every
    // other input, and the weights are normalized.
    const AnimationNode& node = m_BoundGraph->nodes[index];
    const glm::vec2 p(GetParameter(node.parameter), GetParameter(node.parameterY));
    std::vector<float>& weights = m_BlendWeights[index];
    const size_t count = node.positions.size();
    float total = 0.0f;
    for (size_t i = 0; i < count; ++i)
    {
        float weight = 1.0f;
        for (size_t j = 0; j < count && weight > 0.0f; ++j)
        {
            const glm::vec2 edge = node.positions[j] - node.positions[i];
            const float length2 = glm::dot(edge, edge);
            if (j == i || length2 <= 0.0f)
                continue;
            weight = std::min(weight, std::clamp(1.0f - glm::dot(p - node.positions[i], edge) / length2, 0.0f, 1.0f));
        }
        weights[i] = weight;
        total += weight;
    }
    if (total > 0.0f)
    {
        for (float& weight : weights)
            weight /= total;
        return;
    }
    // Outside every band: the nearest input.
    size_t nearest = 0;
    for (size_t i = 1; i < count; ++i)
        if (glm::distance(p, node.positions[i]) < glm::distance(p, node.positions[nearest]))
            nearest = i;
    std::fill(weights.begin(), weights.end(), 0.0f);
    weights[nearest] = 1.0f;
}

void Animator::EvaluateNode(int index, Pose& out, size_t depth)
{
    const AnimationNode& node = m_BoundGraph->nodes[index];
    switch (node.type)
    {
    case AnimationNodeType::Clip:
        SampleInto(m_Nodes[index].clip, m_Nodes[index].time, out);
        return;

    case AnimationNodeType::Blend:
    {
        const float x = WeightOf(node.weight);
        if (x <= 0.0f || x >= 1.0f)
        {
            EvaluateNode(node.inputs[x <= 0.0f ? 0 : 1], out, depth);
            return;
        }
        EvaluateNode(node.inputs[0], out, depth);
        Pose& other = Scratch(depth);
        EvaluateNode(node.inputs[1], other, depth + 1);
        BlendPoses(out, other, x, out);
        return;
    }

    case AnimationNodeType::Blend1D:
    {
        int a, b;
        float t;
        Segment(node, a, b, t);
        EvaluateNode(a, out, depth);
        if (b != a && t > 0.0f)
        {
            Pose& other = Scratch(depth);
            EvaluateNode(b, other, depth + 1);
            BlendPoses(out, other, t, out);
        }
        return;
    }

    case AnimationNodeType::Additive:
    case AnimationNodeType::Layer:
    {
        EvaluateNode(node.inputs[0], out, depth);
        const float x = WeightOf(node.weight);
        if (x <= 0.0f)
            return;

        Pose& layer = Scratch(depth);
        EvaluateNode(node.inputs[1], layer, depth + 1);
        if (node.type == AnimationNodeType::Additive)
        {
            MakeAdditive(layer, m_References[index], layer);
            ApplyAdditive(out, layer, x, out);
        }
        else if (node.layerMode == LayerMode::Additive)
        {
            MakeAdditive(layer, m_References[index], layer);
            ApplyAdditive(out, layer, x, m_Masks[index], out);
        }
        else
        {
            BlendPoses(out, layer, x, m_Masks[index], out);
        }
        return;
    }

    case AnimationNodeType::Blend2D:
    {
        // Weighted inputs one by one, each blended in by its share of the weight so far.
        const std::vector<float>& weights = m_BlendWeights[index];
        float total = 0.0f;
        for (size_t i = 0; i < node.inputs.size(); ++i)
        {
            if (weights[i] <= 0.0f)
                continue;
            if (total == 0.0f)
                EvaluateNode(node.inputs[i], out, depth);
            else
            {
                Pose& other = Scratch(depth);
                EvaluateNode(node.inputs[i], other, depth + 1);
                BlendPoses(out, other, weights[i] / (total + weights[i]), out);
            }
            total += weights[i];
        }
        if (total == 0.0f)
            out.SetToRest(*m_BoundSkeleton);
        return;
    }

    case AnimationNodeType::TwoBoneIK:
        EvaluateNode(node.inputs[0], out, depth);
        SolveTwoBoneIK(index, out);
        return;

    case AnimationNodeType::LookAt:
        EvaluateNode(node.inputs[0], out, depth);
        SolveLookAt(index, out);
        return;

    case AnimationNodeType::StateMachine:
    {
        const std::vector<ActiveState>& active = m_Machines[index].active;
        if (active.empty())
        {
            out.SetToRest(*m_BoundSkeleton);
            return;
        }
        EvaluateNode(node.inputs[active[0].state], out, depth);
        for (size_t i = 1; i < active.size(); ++i)
        {
            Pose& other = Scratch(depth);
            EvaluateNode(node.inputs[active[i].state], other, depth + 1);
            BlendPoses(out, other, active[i].fade, out);
        }
        return;
    }
    }
}

bool Animator::StateChanged(bool useGraph)
{
    m_State.clear();
    if (useGraph)
    {
        m_State.insert(m_State.end(), m_Parameters.begin(), m_Parameters.end());
        if (m_HasIK)
        {
            for (const glm::vec3& v : m_Vectors)
                m_State.insert(m_State.end(), { v.x, v.y, v.z });
            const float* m = &m_ModelFromWorld[0][0];
            m_State.insert(m_State.end(), m, m + 16);
        }
        for (const NodeState& node : m_Nodes)
            m_State.push_back(node.time);
        for (const MachineState& machine : m_Machines)
        {
            for (const ActiveState& active : machine.active)
            {
                m_State.push_back(static_cast<float>(active.state));
                m_State.push_back(active.fade);
            }
        }
    }
    else
    {
        for (const ClipState& state : m_Playing)
        {
            m_State.push_back(static_cast<float>(state.clip));
            m_State.push_back(state.time);
            m_State.push_back(state.fade);
        }
    }
    // Turning root motion on or off changes the pose.
    m_State.push_back(rootMotion ? 1.0f : 0.0f);

    if (m_State == m_Applied)
        return false;
    m_Applied = m_State;
    return true;
}

void Animator::WriteBones()
{
    for (size_t bone = 0; bone < m_Bones.size(); ++bone)
    {
        if (!m_Channels[bone] || !m_Bones[bone].IsValid())
        {
            // Part of the hierarchy was deleted: find the bones again next frame.
            if (m_Channels[bone] && m_Bones[bone].RawId() != 0)
                m_Bound = false;
            continue;
        }

        LocalTRS* trs = m_Bones[bone].Get<LocalTRS>();
        if (!trs)
            continue;

        bool changed = false;
        if (m_Channels[bone] & kTranslation)
            changed |= Assign(trs->pos, &m_Pose.translations[bone].x, 3);
        if (m_Channels[bone] & kRotation)
        {
            const glm::quat& q = m_Pose.rotations[bone];
            const float xyzw[4] = { q.x, q.y, q.z, q.w };
            changed |= Assign(trs->rot_quat, xyzw, 4);
        }
        if (m_Channels[bone] & kScale)
            changed |= Assign(trs->scl, &m_Pose.scales[bone].x, 3);

        if (changed)
            ++trs->version;
    }
}
