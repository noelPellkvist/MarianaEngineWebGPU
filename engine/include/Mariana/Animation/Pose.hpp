#pragma once
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class Skeleton;
class AnimationClip;
struct ClipBinding;

// A local transform per bone of a skeleton (indexed like Skeleton::Bones). What clips sample into,
// what blends mix, and what an Animator writes to the bones' entities.
struct Pose
{
    std::vector<glm::vec3> translations;
    std::vector<glm::quat> rotations;
    std::vector<glm::vec3> scales;

    size_t BoneCount() const { return rotations.size(); }
    // Sized to the skeleton, every bone at its rest transform.
    void SetToRest(const Skeleton& skeleton);
};

// Writes what the clip animates at `time` into the bones `binding` found; the rest of the pose is
// left alone, so start from SetToRest for a full pose.
void SampleClip(const AnimationClip& clip, const ClipBinding& binding, float time, Pose& pose);

// out = a moved toward b by `weight` (0 = a, 1 = b): translations and scales lerped, rotations
// nlerped along the shorter arc. out may be a or b; the poses must have the same bone count.
void BlendPoses(const Pose& a, const Pose& b, float weight, Pose& out);
// The same with each bone's weight scaled by `mask` (one per bone, BoneMask::Resolve).
void BlendPoses(const Pose& a, const Pose& b, float weight, const std::vector<float>& mask, Pose& out);

// Additive poses: out = how `pose` differs from `reference` (translation and scale offsets, the
// rotation that takes the reference's to the pose's), to be added to another pose.
void MakeAdditive(const Pose& pose, const Pose& reference, Pose& out);
// out = base with `weight` of `additive` added on top (optionally per bone by `mask`).
void ApplyAdditive(const Pose& base, const Pose& additive, float weight, Pose& out);
void ApplyAdditive(const Pose& base, const Pose& additive, float weight, const std::vector<float>& mask, Pose& out);
