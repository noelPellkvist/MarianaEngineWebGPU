#include <Mariana/Animation/AnimationClip.hpp>
#include <Mariana/Animation/Skeleton.hpp>

#include <algorithm>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/quaternion.hpp>

glm::vec4 AnimationCurve::Sample(float time, bool rotation) const
{
    const glm::vec4 fallback = rotation ? glm::vec4(0.0f, 0.0f, 0.0f, 1.0f) : glm::vec4(0.0f);
    if (Empty())
        return fallback;

    auto normalized = [&](glm::vec4 value) {
        if (!rotation)
            return value;
        const float length = glm::length(value);
        return length > 0.0f ? value / length : fallback;
    };

    // The key at or before `time`.
    const size_t count = times.size();
    if (count == 1 || time <= times.front())
        return normalized(values.front());
    if (time >= times.back())
        return normalized(values[count - 1]);
    const size_t k = static_cast<size_t>(std::upper_bound(times.begin(), times.end(), time) - times.begin()) - 1;
    if (time == times[k])
        return normalized(values[k]);

    const float span = times[k + 1] - times[k];
    const float t = span > 0.0f ? std::clamp((time - times[k]) / span, 0.0f, 1.0f) : 0.0f;
    const glm::vec4& p0 = values[k];
    const glm::vec4& p1 = values[k + 1];

    switch (interpolation)
    {
    case Interpolation::Step:
        return normalized(p0);

    case Interpolation::Linear:
        if (rotation)
        {
            const glm::quat q = glm::slerp(glm::quat(p0.w, p0.x, p0.y, p0.z), glm::quat(p1.w, p1.x, p1.y, p1.z), t);
            return normalized(glm::vec4(q.x, q.y, q.z, q.w));
        }
        return glm::mix(p0, p1, t);

    case Interpolation::CubicSpline:
    {
        // glTF cubic spline: a Hermite curve with tangents scaled by the keyframe span.
        const float t2 = t * t;
        const float t3 = t2 * t;
        const glm::vec4 m0 = (k < outTangents.size() ? outTangents[k] : glm::vec4(0.0f)) * span;
        const glm::vec4 m1 = (k + 1 < inTangents.size() ? inTangents[k + 1] : glm::vec4(0.0f)) * span;
        return normalized((2.0f * t3 - 3.0f * t2 + 1.0f) * p0
                        + (t3 - 2.0f * t2 + t) * m0
                        + (-2.0f * t3 + 3.0f * t2) * p1
                        + (t3 - t2) * m1);
    }
    }
    return normalized(p0);
}

AnimationClip& AnimationClip::Build()
{
    m_Duration = 0.0f;
    for (const AnimationTrack& track : tracks)
        for (const AnimationCurve* curve : { &track.translation, &track.rotation, &track.scale })
            if (!curve->times.empty())
                m_Duration = std::max(m_Duration, curve->times.back());
    return *this;
}

ClipBinding ClipBinding::Bind(const AnimationClip& clip, const Skeleton& skeleton)
{
    // A clip made with the skeleton's own file targets its nodes; others match bone names.
    const bool sameFile = !clip.source.empty() && clip.source == skeleton.source;
    ClipBinding binding;
    binding.bones.reserve(clip.tracks.size());
    for (const AnimationTrack& track : clip.tracks)
    {
        int bone = sameFile && track.node != ~0u ? skeleton.FindBoneByNode(track.node) : -1;
        if (bone < 0)
            bone = skeleton.FindBone(track.bone);
        binding.bones.push_back(bone);
        binding.bound += bone >= 0;
    }
    return binding;
}
