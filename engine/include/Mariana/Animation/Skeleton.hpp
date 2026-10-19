#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Marks an entity as glTF node `nodeIndex` of its model: the entity that bone (Skeleton::Bone::node)
// and skin joints resolve to.
struct AnimationTargetEntity
{
    uint32_t nodeIndex = 0;
};

// A bone's transform relative to its parent.
struct BoneTransform
{
    glm::vec3 translation{ 0.0f };
    glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
    glm::vec3 scale{ 1.0f };
};

// The bones of a model, parents before children, each with its rest pose (what poses start from).
// The glTF loader makes one per file from the scene's nodes, keyed "<file>#skeleton"; `source` is that
// file's key, so a clip from the same file binds to it by node and one from elsewhere by bone name.
class Skeleton
{
public:
    struct Bone
    {
        std::string name;
        int parent = -1;     // parent bone, -1 for a root
        uint32_t node = ~0u; // the glTF node it was made from
        BoneTransform rest;
    };

    std::string source;

    // Its parent must have been added first (or be -1).
    void AddBone(Bone bone);

    size_t BoneCount() const { return m_Bones.size(); }
    const Bone& GetBone(size_t index) const { return m_Bones[index]; }
    const std::vector<Bone>& Bones() const { return m_Bones; }

    // -1 when there is none. Names match without a "namespace:" prefix (Mixamo's "mixamorig:").
    int FindBone(std::string_view name) const;
    int FindBoneByNode(uint32_t node) const;
    // Whether `bone` is `ancestor` or below it.
    bool IsDescendant(int bone, int ancestor) const;

    // What identifies a bone across files: its name without a "namespace:" prefix.
    static std::string_view BaseName(std::string_view name);

private:
    std::vector<Bone> m_Bones;
    std::unordered_map<std::string, int> m_ByName;
    std::unordered_map<uint32_t, int> m_ByNode;
};
