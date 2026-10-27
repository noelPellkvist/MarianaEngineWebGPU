#pragma once
// Models built in code for the animation scenarios (MarianaEngineTest, MarianaPhysicsTest): a
// skeleton (an asset), an entity per bone below a root (with AnimationTargetEntity, as the glTF
// loader makes them) and clips with linear keys.
#include <Mariana/Animation/Animator.hpp>
#include <Mariana/Assets/AssetManager.hpp>
#include <Mariana/Scene/ECS.hpp>

#include <glm/gtc/quaternion.hpp>

#include <memory>
#include <string>
#include <vector>

namespace TestRig
{
    struct BoneSpec
    {
        std::string name;
        int parent = -1; // a bone before it; -1 = the root's child
        glm::vec3 translation{ 0.0f };
        glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
        glm::vec3 scale{ 1.0f };
    };

    struct Rig
    {
        Entity root;
        std::vector<Entity> bones;
        AssetHandle<Skeleton> skeleton;
    };

    // The bones below `root` (an Animator goes on it), and the skeleton they make.
    inline Rig Make(Scene& scene, AssetManager2& assets, Entity root, const std::vector<BoneSpec>& specs, const std::string& key)
    {
        auto skeleton = std::make_shared<Skeleton>();
        skeleton->source = key;
        Rig rig;
        rig.root = root;
        for (size_t i = 0; i < specs.size(); ++i)
        {
            const BoneSpec& spec = specs[i];
            Skeleton::Bone bone;
            bone.name = spec.name;
            bone.parent = spec.parent;
            bone.node = static_cast<uint32_t>(i);
            bone.rest = { spec.translation, spec.rotation, spec.scale };
            skeleton->AddBone(bone);

            Entity e = scene.Instantiate(spec.name.c_str());
            e.SetParent(spec.parent >= 0 ? rig.bones[spec.parent] : root);
            e.SetPosition(spec.translation.x, spec.translation.y, spec.translation.z);
            e.SetRotationQuat(spec.rotation.x, spec.rotation.y, spec.rotation.z, spec.rotation.w);
            e.SetScale(spec.scale.x, spec.scale.y, spec.scale.z);
            e.Add<AnimationTargetEntity>(AnimationTargetEntity{ static_cast<uint32_t>(i) });
            rig.bones.push_back(e);
        }
        rig.skeleton = assets.RegisterAsset<Skeleton>(skeleton, AssetType::Skeleton, key, key + "#skeleton");
        return rig;
    }

    // A track moving one bone (by name): positions and/or rotations at `times`, linear.
    inline AnimationTrack Track(const std::string& bone, const std::vector<float>& times, const std::vector<glm::vec3>& positions,
                                const std::vector<glm::quat>& rotations = {})
    {
        AnimationTrack track;
        track.bone = bone;
        if (!positions.empty())
        {
            track.translation.times = times;
            for (const glm::vec3& p : positions)
                track.translation.values.emplace_back(p, 0.0f);
        }
        if (!rotations.empty())
        {
            track.rotation.times = times;
            for (const glm::quat& q : rotations)
                track.rotation.values.emplace_back(q.x, q.y, q.z, q.w);
        }
        return track;
    }

    inline NamedClip Clip(AssetManager2& assets, const std::string& name, std::vector<AnimationTrack> tracks)
    {
        auto clip = std::make_shared<AnimationClip>(name);
        clip->tracks = std::move(tracks);
        clip->Build();
        return { name, assets.RegisterAsset<AnimationClip>(clip, AssetType::Animation, name, "test://clip/" + name) };
    }

    inline glm::quat Yaw(float degrees)
    {
        return glm::angleAxis(glm::radians(degrees), glm::vec3(0.0f, 1.0f, 0.0f));
    }

    // A walker: Armature (scaled to centimetres, as Mixamo's exports are) > Hips 95 cm up, legs.
    // Its clips' hips move along +Z at `metresPerSecond` over one second.
    inline std::vector<BoneSpec> Walker()
    {
        return { { "Armature", -1, glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(0.01f) },
                 { "mixamorig:Hips", 0, glm::vec3(0.0f, 95.0f, 0.0f) },
                 { "mixamorig:LeftUpLeg", 1, glm::vec3(10.0f, -5.0f, 0.0f) },
                 { "mixamorig:RightUpLeg", 1, glm::vec3(-10.0f, -5.0f, 0.0f) } };
    }

    inline NamedClip WalkClip(AssetManager2& assets, const std::string& name, float metresPerSecond)
    {
        const float cm = metresPerSecond * 100.0f;
        return Clip(assets, name, { Track("mixamorig:Hips", { 0.0f, 0.5f, 1.0f }, { glm::vec3(0.0f, 95.0f, 0.0f), glm::vec3(0.0f, 97.0f, 0.5f * cm), glm::vec3(0.0f, 95.0f, cm) }) });
    }
}
