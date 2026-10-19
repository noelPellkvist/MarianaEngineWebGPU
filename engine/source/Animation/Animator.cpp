#include <Mariana/Animation/Animator.hpp>

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
      speed(other.speed),
      playing(other.playing),
      m_Clip(other.m_Clip),
      m_Time(other.m_Time),
      m_Loop(other.m_Loop)
{
}

Animator& Animator::operator=(const Animator& other)
{
    if (this != &other)
        *this = Animator(other);
    return *this;
}

const std::shared_ptr<AnimationClip>& Animator::GetCurrentClip() const
{
    static const std::shared_ptr<AnimationClip> none;
    if (m_Clip < 0 || m_Clip >= static_cast<int>(clips.size()))
        return none;
    return clips[m_Clip].Get();
}

void Animator::Play(int clip, bool loop)
{
    if (clip < 0 || clip >= static_cast<int>(clips.size()))
        return;

    m_Clip = clip;
    m_Time = 0.0f;
    m_Loop = loop;
    playing = true;
}

size_t Animator::BoundTracks() const
{
    return m_Clip >= 0 && m_Clip < static_cast<int>(m_Bindings.size()) ? m_Bindings[m_Clip].bound : 0;
}

bool Animator::NeedsBind() const
{
    if (m_BoundSkeleton != skeleton.Get().get() || m_BoundClips.size() != clips.size())
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

    m_Pose.SetToRest(bones);
    m_AppliedClip = -1;
    m_AppliedTime = -1.0f;
}

void Animator::Update(Entity root, float dt)
{
    const std::shared_ptr<AnimationClip>& clip = GetCurrentClip();
    if (!clip || !skeleton.Get())
        return;

    if (NeedsBind())
        Bind(root);

    if (playing)
        m_Time += dt * speed;

    const float duration = clip->GetDuration();
    if (duration <= 0.0f)
    {
        m_Time = 0.0f;
    }
    else if (m_Loop)
    {
        m_Time = std::fmod(m_Time, duration);
        if (m_Time < 0.0f)
            m_Time += duration;
    }
    else
    {
        m_Time = std::clamp(m_Time, 0.0f, duration);
    }

    // Paused or held at the end: the pose is already written.
    if (m_Clip == m_AppliedClip && m_Time == m_AppliedTime)
        return;
    m_AppliedClip = m_Clip;
    m_AppliedTime = m_Time;

    m_Pose.SetToRest(*m_BoundSkeleton);
    SampleClip(*clip, m_Bindings[m_Clip], m_Time, m_Pose);
    WriteBones();
}

void Animator::WriteBones()
{
    for (size_t bone = 0; bone < m_Bones.size(); ++bone)
    {
        if (!m_Channels[bone] || !m_Bones[bone].IsValid())
        {
            // Part of the hierarchy was deleted: find the bones again next frame.
            if (m_Channels[bone] && m_Bones[bone].RawId() != 0)
                m_BoundSkeleton = nullptr;
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
