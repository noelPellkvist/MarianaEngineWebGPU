#include <Mariana/Animation/Pose.hpp>
#include <Mariana/Animation/AnimationClip.hpp>
#include <Mariana/Animation/Skeleton.hpp>

#include <algorithm>

void Pose::SetToRest(const Skeleton& skeleton)
{
    const size_t count = skeleton.BoneCount();
    translations.resize(count);
    rotations.resize(count);
    scales.resize(count);
    for (size_t i = 0; i < count; ++i)
    {
        const BoneTransform& rest = skeleton.GetBone(i).rest;
        translations[i] = rest.translation;
        rotations[i] = rest.rotation;
        scales[i] = rest.scale;
    }
}

void SampleClip(const AnimationClip& clip, const ClipBinding& binding, float time, Pose& pose)
{
    const size_t count = std::min(clip.tracks.size(), binding.bones.size());
    for (size_t i = 0; i < count; ++i)
    {
        const int bone = binding.bones[i];
        if (bone < 0 || static_cast<size_t>(bone) >= pose.BoneCount())
            continue;

        const AnimationTrack& track = clip.tracks[i];
        if (!track.translation.Empty())
            pose.translations[bone] = glm::vec3(track.translation.Sample(time, false));
        if (!track.rotation.Empty())
        {
            const glm::vec4 q = track.rotation.Sample(time, true);
            pose.rotations[bone] = glm::quat(q.w, q.x, q.y, q.z);
        }
        if (!track.scale.Empty())
            pose.scales[bone] = glm::vec3(track.scale.Sample(time, false));
    }
}

void BlendPoses(const Pose& a, const Pose& b, float weight, Pose& out)
{
    const size_t count = a.BoneCount();
    out.translations.resize(count);
    out.rotations.resize(count);
    out.scales.resize(count);
    for (size_t i = 0; i < count; ++i)
    {
        out.translations[i] = glm::mix(a.translations[i], b.translations[i], weight);
        out.scales[i] = glm::mix(a.scales[i], b.scales[i], weight);

        const glm::quat& qa = a.rotations[i];
        const glm::quat qb = glm::dot(qa, b.rotations[i]) < 0.0f ? -b.rotations[i] : b.rotations[i];
        out.rotations[i] = glm::normalize(qa * (1.0f - weight) + qb * weight);
    }
}
