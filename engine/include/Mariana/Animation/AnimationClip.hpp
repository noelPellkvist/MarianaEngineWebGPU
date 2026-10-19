#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <glm/glm.hpp>

class Skeleton;

enum class Interpolation { Step, Linear, CubicSpline };

// Keyframes of one property: a translation or scale (xyz) or a rotation (quaternion xyzw).
struct AnimationCurve
{
    Interpolation interpolation = Interpolation::Linear;
    std::vector<float> times;
    std::vector<glm::vec4> values;
    // CubicSpline only, one per key (glTF's in and out tangents).
    std::vector<glm::vec4> inTangents;
    std::vector<glm::vec4> outTangents;

    bool Empty() const { return times.empty() || values.size() < times.size(); }
    // The value at `time`, held at the first and last keys. Rotations are slerped (linear) and come
    // back normalized.
    glm::vec4 Sample(float time, bool rotation) const;
};

// How one bone moves. A clip's tracks find their bones by name, or by node in the clip's own file.
struct AnimationTrack
{
    std::string bone;
    uint32_t node = ~0u;
    AnimationCurve translation;
    AnimationCurve rotation;
    AnimationCurve scale;
};

// An animation, independent of any model: a track per animated bone, sampled into a Pose
// (Pose.hpp). A ClipBinding says which bone of a skeleton each track drives.
class AnimationClip
{
public:
    explicit AnimationClip(std::string name = {}) : m_Name(std::move(name)) {}

    const std::string& GetName() const { return m_Name; }
    float GetDuration() const { return m_Duration; }

    // The key of the file it came from: a skeleton from the same file binds it by node.
    std::string source;
    std::vector<AnimationTrack> tracks;

    // Sets the duration from the last key of any curve; call after filling the tracks.
    AnimationClip& Build();

private:
    std::string m_Name;
    float m_Duration = 0.0f;
};

// Which bone of a skeleton each track of a clip moves (-1: none of them).
struct ClipBinding
{
    std::vector<int> bones;
    size_t bound = 0; // tracks that found their bone

    static ClipBinding Bind(const AnimationClip& clip, const Skeleton& skeleton);
};
