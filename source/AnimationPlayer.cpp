#include <AnimationPlayer.hpp>

#include <algorithm>
#include <cmath>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace
{
    // Index k of the keyframe at or before `time`. Starts from the channel's cursor and only
    // rewinds when time went backwards (looping), so a playing clip samples in O(1).
    size_t FindKeyframe(const std::vector<AnimationKeyFrame>& keys, size_t& cursor, float time)
    {
        if (cursor >= keys.size() || keys[cursor].time > time)
            cursor = 0;

        while (cursor + 1 < keys.size() && keys[cursor + 1].time <= time)
            ++cursor;

        return cursor;
    }

    glm::vec4 ToVec4(const std::vector<float>& values, glm::vec4 fallback)
    {
        for (size_t i = 0; i < 4 && i < values.size(); ++i)
            fallback[static_cast<glm::length_t>(i)] = values[i];
        return fallback;
    }

    // Samples a translation/scale (xyz) or rotation (quaternion xyzw) channel at `time`.
    glm::vec4 SampleChannel(const AnimationChannel& channel, size_t& cursor, float time, bool isRotation)
    {
        const glm::vec4 fallback = isRotation ? glm::vec4(0.0f, 0.0f, 0.0f, 1.0f) : glm::vec4(0.0f);
        const std::vector<AnimationKeyFrame>& keys = channel.keyFrames;

        const size_t k = FindKeyframe(keys, cursor, time);
        if (k + 1 >= keys.size() || time <= keys[k].time)
            return ToVec4(keys[k].data, fallback);

        const AnimationKeyFrame& a = keys[k];
        const AnimationKeyFrame& b = keys[k + 1];
        const float span = b.time - a.time;
        const float t = span > 0.0f ? std::clamp((time - a.time) / span, 0.0f, 1.0f) : 0.0f;

        const glm::vec4 p0 = ToVec4(a.data, fallback);
        const glm::vec4 p1 = ToVec4(b.data, fallback);

        switch (channel.interpolation)
        {
        case AnimationInterpolationType::Step:
            return p0;

        case AnimationInterpolationType::Linear:
            if (isRotation)
            {
                const glm::quat q = glm::slerp(glm::quat(p0.w, p0.x, p0.y, p0.z),
                                               glm::quat(p1.w, p1.x, p1.y, p1.z), t);
                return glm::vec4(q.x, q.y, q.z, q.w);
            }
            return glm::mix(p0, p1, t);

        case AnimationInterpolationType::CubicSpline:
        {
            // glTF cubic spline: Hermite curve with tangents scaled by the keyframe span.
            const float t2 = t * t;
            const float t3 = t2 * t;
            const glm::vec4 m0 = ToVec4(a.outTangent, glm::vec4(0.0f)) * span;
            const glm::vec4 m1 = ToVec4(b.inTangent, glm::vec4(0.0f)) * span;

            glm::vec4 value = (2.0f * t3 - 3.0f * t2 + 1.0f) * p0
                            + (t3 - 2.0f * t2 + t) * m0
                            + (-2.0f * t3 + 3.0f * t2) * p1
                            + (t3 - t2) * m1;

            if (isRotation)
            {
                const float length = glm::length(value);
                value = length > 0.0f ? value / length : fallback;
            }
            return value;
        }
        }

        return p0;
    }
}


namespace
{
    struct NodeChannels { int translation = -1; int rotation = -1; int scale = -1; };

    void CollectTargets(Entity e, const std::vector<NodeChannels>& nodes, bool isRoot,
                        std::vector<AnimationPlayer::Target>& targets)
    {
        if (!isRoot && e.Has<AnimationPlayer>())
            return;

        if (const AnimationTargetEntity* target = e.TryGet<AnimationTargetEntity>())
        {
            if (target->nodeIndex < nodes.size())
            {
                const NodeChannels& node = nodes[target->nodeIndex];
                if (node.translation >= 0 || node.rotation >= 0 || node.scale >= 0)
                    targets.push_back({ e, node.translation, node.rotation, node.scale });
            }
        }

        e.ForEachChild([&](Entity child) {
            CollectTargets(child, nodes, false, targets);
        });
    }

    // Writes `count` floats only if they differ, so unchanged channels don't dirty the transform.
    bool Assign(float* dst, const glm::vec4& src, int count)
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
}

AnimationPlayer::AnimationPlayer(const AnimationPlayer& other)
    : clips(other.clips),
      currentClip(other.currentClip),
      time(other.time),
      speed(other.speed),
      playing(other.playing),
      loop(other.loop)
{
}

AnimationPlayer& AnimationPlayer::operator=(const AnimationPlayer& other)
{
    if (this == &other)
        return *this;

    clips = other.clips;
    currentClip = other.currentClip;
    time = other.time;
    speed = other.speed;
    playing = other.playing;
    loop = other.loop;
    targets.clear();
    cursors.clear();
    boundClip = -1;
    appliedTime = -1.0f;
    return *this;
}

const std::shared_ptr<Animation>& AnimationPlayer::GetCurrentClip() const
{
    static const std::shared_ptr<Animation> none;
    if (currentClip < 0 || currentClip >= static_cast<int>(clips.size()))
        return none;

    return clips[currentClip].Get();
}

void AnimationPlayer::Play(int clipIndex)
{
    if (clipIndex < 0 || clipIndex >= static_cast<int>(clips.size()))
        return;

    currentClip = clipIndex;
    time = 0.0f;
    playing = true;
    boundClip = -1;
}

void AnimationPlayer::Bind(Entity root, const Animation& clip)
{
    std::vector<NodeChannels> nodes(static_cast<size_t>(std::max(clip.GetNodeCount(), 0)));

    for (size_t i = 0; i < clip.GetChannelCount(); ++i)
    {
        const AnimationChannel& channel = clip.GetChannel(i);
        if (channel.targetNodeIndex < 0 || channel.keyFrames.empty())
            continue;

        const size_t node = static_cast<size_t>(channel.targetNodeIndex);
        if (node >= nodes.size())
            nodes.resize(node + 1);

        switch (channel.type)
        {
        case AnimationChannelType::Translation: nodes[node].translation = static_cast<int>(i); break;
        case AnimationChannelType::Rotation:    nodes[node].rotation = static_cast<int>(i); break;
        case AnimationChannelType::Scale:       nodes[node].scale = static_cast<int>(i); break;
        default: break; // Morph target weights aren't supported.
        }
    }

    // Walk the hierarchy once here instead of every frame; only animated nodes are kept.
    targets.clear();
    CollectTargets(root, nodes, true, targets);

    cursors.assign(clip.GetChannelCount(), 0);
    boundClip = currentClip;
    appliedTime = -1.0f;
}

void AnimationPlayer::Update(Entity root, float dt)
{
    const std::shared_ptr<Animation>& clip = GetCurrentClip();
    if (!clip)
        return;

    if (boundClip != currentClip || cursors.size() != clip->GetChannelCount())
        Bind(root, *clip);

    if (playing)
        time += dt * speed;

    const float duration = clip->GetDuration();
    if (duration <= 0.0f)
    {
        time = 0.0f;
    }
    else if (loop)
    {
        time = std::fmod(time, duration);
        if (time < 0.0f)
            time += duration;
    }
    else
    {
        time = std::clamp(time, 0.0f, duration);
    }

    // Paused or held at the end: the pose is already applied.
    if (time == appliedTime)
        return;
    appliedTime = time;

    for (const Target& target : targets)
    {
        if (!target.entity.IsValid())
        {
            // Part of the hierarchy was deleted; collect the targets again next frame.
            boundClip = -1;
            continue;
        }
        ApplyToTarget(target, *clip);
    }
}

void AnimationPlayer::ApplyToTarget(const Target& target, const Animation& clip)
{
    Entity entity = target.entity;
    LocalTRS* trs = entity.Get<LocalTRS>();
    if (!trs)
        return;

    bool changed = false;
    if (target.translation >= 0)
    {
        const glm::vec4 p = SampleChannel(clip.GetChannel(target.translation), cursors[target.translation], time, false);
        changed |= Assign(trs->pos, p, 3);
    }
    if (target.rotation >= 0)
    {
        glm::vec4 q = SampleChannel(clip.GetChannel(target.rotation), cursors[target.rotation], time, true);
        const float length = glm::length(q);
        q = length > 0.0f ? q / length : glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        changed |= Assign(trs->rot_quat, q, 4);
    }
    if (target.scale >= 0)
    {
        const glm::vec4 s = SampleChannel(clip.GetChannel(target.scale), cursors[target.scale], time, false);
        changed |= Assign(trs->scl, s, 3);
    }

    if (changed)
        ++trs->version;
}
