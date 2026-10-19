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

namespace
{
    void Resize(Pose& pose, size_t count)
    {
        pose.translations.resize(count);
        pose.rotations.resize(count);
        pose.scales.resize(count);
    }

    void BlendBone(const Pose& a, const Pose& b, size_t i, float weight, Pose& out)
    {
        out.translations[i] = glm::mix(a.translations[i], b.translations[i], weight);
        out.scales[i] = glm::mix(a.scales[i], b.scales[i], weight);

        const glm::quat& qa = a.rotations[i];
        const glm::quat qb = glm::dot(qa, b.rotations[i]) < 0.0f ? -b.rotations[i] : b.rotations[i];
        out.rotations[i] = glm::normalize(qa * (1.0f - weight) + qb * weight);
    }

    void AddBone(const Pose& base, const Pose& additive, size_t i, float weight, Pose& out)
    {
        out.translations[i] = base.translations[i] + additive.translations[i] * weight;
        out.scales[i] = base.scales[i] * glm::mix(glm::vec3(1.0f), additive.scales[i], weight);

        // A part of the additive rotation: nlerp from identity, along the shorter arc.
        glm::quat delta = additive.rotations[i];
        if (delta.w < 0.0f)
            delta = -delta;
        delta = glm::normalize(glm::quat(1.0f, 0.0f, 0.0f, 0.0f) * (1.0f - weight) + delta * weight);
        out.rotations[i] = glm::normalize(base.rotations[i] * delta);
    }
}

void BlendPoses(const Pose& a, const Pose& b, float weight, Pose& out)
{
    const size_t count = a.BoneCount();
    Resize(out, count);
    for (size_t i = 0; i < count; ++i)
        BlendBone(a, b, i, weight, out);
}

void BlendPoses(const Pose& a, const Pose& b, float weight, const std::vector<float>& mask, Pose& out)
{
    const size_t count = a.BoneCount();
    Resize(out, count);
    for (size_t i = 0; i < count; ++i)
    {
        const float w = i < mask.size() ? weight * mask[i] : 0.0f;
        if (w > 0.0f)
            BlendBone(a, b, i, w, out);
        else if (&out != &a)
        {
            out.translations[i] = a.translations[i];
            out.rotations[i] = a.rotations[i];
            out.scales[i] = a.scales[i];
        }
    }
}

void MakeAdditive(const Pose& pose, const Pose& reference, Pose& out)
{
    const size_t count = pose.BoneCount();
    Resize(out, count);
    for (size_t i = 0; i < count; ++i)
    {
        out.translations[i] = pose.translations[i] - reference.translations[i];
        const glm::vec3& s = reference.scales[i];
        out.scales[i] = glm::vec3(s.x != 0.0f ? pose.scales[i].x / s.x : 1.0f,
                                  s.y != 0.0f ? pose.scales[i].y / s.y : 1.0f,
                                  s.z != 0.0f ? pose.scales[i].z / s.z : 1.0f);
        out.rotations[i] = glm::normalize(glm::inverse(reference.rotations[i]) * pose.rotations[i]);
    }
}

void ApplyAdditive(const Pose& base, const Pose& additive, float weight, Pose& out)
{
    const size_t count = base.BoneCount();
    Resize(out, count);
    for (size_t i = 0; i < count; ++i)
        AddBone(base, additive, i, weight, out);
}

void ApplyAdditive(const Pose& base, const Pose& additive, float weight, const std::vector<float>& mask, Pose& out)
{
    const size_t count = base.BoneCount();
    Resize(out, count);
    for (size_t i = 0; i < count; ++i)
    {
        const float w = i < mask.size() ? weight * mask[i] : 0.0f;
        if (w > 0.0f)
            AddBone(base, additive, i, w, out);
        else if (&out != &base)
        {
            out.translations[i] = base.translations[i];
            out.rotations[i] = base.rotations[i];
            out.scales[i] = base.scales[i];
        }
    }
}
