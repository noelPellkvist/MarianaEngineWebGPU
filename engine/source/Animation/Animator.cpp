#include <Mariana/Animation/Animator.hpp>
#include <Mariana/Foundation/Logger.hpp>

#include <algorithm>
#include <cmath>

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
}

void Animator::RegisterComponents()
{
    ECS::RegisterComponent<AnimationTargetEntity>("AnimationTargetEntity");
    ECS::RegisterComponent<Animator>("Animator");
}

System Animator::CreateSystem(Scene& scene)
{
    return scene.CreateSystem<Animator>([](Entity e, Animator& animator, float dt) {
        animator.Update(e, dt);
    });
}

Animator::Animator(const Animator& other)
    : skeleton(other.skeleton),
      clips(other.clips),
      graph(other.graph),
      speed(other.speed),
      playing(other.playing),
      m_Playing(other.m_Playing),
      m_ParameterGraph(other.m_ParameterGraph),
      m_Parameters(other.m_Parameters)
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
    for (size_t i = 0; i < clips.size(); ++i)
        if (clips[i].Get() && clips[i].Get()->GetName() == name)
            return static_cast<int>(i);
    return -1;
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
    return clip >= 0 && clip < static_cast<int>(clips.size()) ? clips[clip].Get() : none;
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
    if (definition == m_ParameterGraph && (!definition || m_Parameters.size() == definition->parameters.size()))
        return;

    m_ParameterGraph = definition;
    m_Parameters.clear();
    if (definition)
        for (const AnimationParameter& parameter : definition->parameters)
            m_Parameters.push_back(parameter.defaultValue);
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

size_t Animator::BoundTracks() const
{
    const int clip = CurrentClip();
    return clip >= 0 && clip < static_cast<int>(m_Bindings.size()) ? m_Bindings[clip].bound : 0;
}

bool Animator::NeedsBind() const
{
    if (!m_Bound || m_BoundSkeleton != skeleton.Get().get() || m_BoundGraph != graph.Get().get() || m_BoundClips.size() != clips.size())
        return true;
    for (size_t i = 0; i < clips.size(); ++i)
        if (m_BoundClips[i] != clips[i].Get().get())
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
    for (const AssetHandle<AnimationClip>& handle : clips)
    {
        const AnimationClip* clip = handle.Get().get();
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
    std::string error;
    m_GraphValid = definition && definition->Validate(error);
    if (definition && !m_GraphValid)
        Logger::Error("Animator: its graph can't be used: " + error);
    if (m_GraphValid)
        BindGraph(*definition);
    else
    {
        m_Nodes.clear();
        m_SyncGroupOf.clear();
        m_SyncGroups.clear();
        m_Masks.clear();
        m_References.clear();
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
        m_SyncGroups.clear();
    }
    for (SyncGroup& group : m_SyncGroups)
        group.members.clear();
    m_SyncGroupOf.assign(count, -1);
    m_Masks.assign(count, {});
    m_References.assign(count, {});

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
        SampleClip(*m_BoundClips[clip], m_Bindings[clip], time, pose);
}

Pose& Animator::Scratch(size_t depth)
{
    // Sized by Bind: evaluation goes at most one level deeper per node.
    return m_Scratch[std::min(depth, m_Scratch.size() - 1)];
}

void Animator::Update(Entity root, float dt)
{
    if (!skeleton.Get())
        return;
    if (!graph.Get() && m_Playing.empty())
        return;

    if (NeedsBind())
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

    // Paused or held at the end: the pose is already written.
    if (!StateChanged(useGraph))
        return;

    if (useGraph)
        EvaluateNode(m_BoundGraph->output, m_Pose, 0);
    else
        EvaluateClips();
    WriteBones();
}

void Animator::AdvanceClips(float delta)
{
    for (ClipState& state : m_Playing)
    {
        const AnimationClip* clip = state.clip < static_cast<int>(m_BoundClips.size()) ? m_BoundClips[state.clip] : nullptr;
        state.time = Advance(state.time, delta, clip ? clip->GetDuration() : 0.0f, state.loop);
    }

    // Only the newest clip fades; one that was fading in when another started stays where it got to.
    ClipState& newest = m_Playing.back();
    if (newest.fade < 1.0f)
        newest.fade = newest.fadeDuration > 0.0f ? std::min(1.0f, newest.fade + std::abs(delta) / newest.fadeDuration) : 1.0f;

    float remaining = 1.0f;
    for (size_t i = m_Playing.size(); i-- > 0;)
    {
        const float fade = i == 0 ? 1.0f : m_Playing[i].fade;
        m_Playing[i].weight = remaining * fade;
        remaining *= 1.0f - fade;
    }
    // Clips under one that has fully faded in no longer show.
    m_Playing.erase(std::remove_if(m_Playing.begin(), m_Playing.end() - 1, [](const ClipState& s) { return s.weight <= 0.0f; }), m_Playing.end() - 1);
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

void Animator::Weigh(int index, float weight)
{
    if (weight <= 0.0f)
        return;
    m_Nodes[index].weight += weight;

    const AnimationNode& node = m_BoundGraph->nodes[index];
    switch (node.type)
    {
    case AnimationNodeType::Clip:
        break;
    case AnimationNodeType::Blend:
    {
        const float x = WeightOf(node.weight);
        Weigh(node.inputs[0], weight * (1.0f - x));
        Weigh(node.inputs[1], weight * x);
        break;
    }
    case AnimationNodeType::Blend1D:
    {
        int a, b;
        float t;
        Segment(node, a, b, t);
        Weigh(a, weight * (1.0f - t));
        if (b != a)
            Weigh(b, weight * t);
        break;
    }
    case AnimationNodeType::Additive:
    case AnimationNodeType::Layer:
        Weigh(node.inputs[0], weight);
        Weigh(node.inputs[1], weight * WeightOf(node.weight));
        break;
    }
}

void Animator::AdvanceGraph(float delta)
{
    const AnimationGraph& definition = *m_BoundGraph;
    for (NodeState& state : m_Nodes)
        state.weight = 0.0f;
    Weigh(definition.output, 1.0f);

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
    }
}

bool Animator::StateChanged(bool useGraph)
{
    m_State.clear();
    if (useGraph)
    {
        m_State.insert(m_State.end(), m_Parameters.begin(), m_Parameters.end());
        for (const NodeState& node : m_Nodes)
            m_State.push_back(node.time);
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
