#pragma once
#include <Mariana/Scene/ECS.hpp>
#include <Mariana/Animation/AnimationClip.hpp>
#include <Mariana/Animation/Pose.hpp>
#include <Mariana/Animation/Skeleton.hpp>
#include <Mariana/Assets/AssetManager.hpp>
#include <memory>
#include <vector>

// Animates a model: goes on its root (the glTF loader's wrapper entity) and moves the bones below
// it, the entities whose AnimationTargetEntity is a bone of `skeleton`. Its clips sample into a
// pose, which is written to the bones' LocalTRS: only the channels some clip animates, and only
// values that changed. Other models below it (another Animator, or an entity that isn't a bone)
// are left alone.
struct Animator
{
    AssetHandle<Skeleton> skeleton;
    std::vector<AssetHandle<AnimationClip>> clips;
    float speed = 1.0f;
    bool playing = true;

    Animator() = default;
    // Copies the settings and what plays; the bones it found belong to the source's entities.
    Animator(const Animator& other);
    Animator& operator=(const Animator& other);
    Animator(Animator&&) = default;
    Animator& operator=(Animator&&) = default;

    // Animator and AnimationTargetEntity; call before Scene::UpdateComponentRegistry.
    static void RegisterComponents();
    // Updates every Animator of the scene by the delta given to System::Run (game time).
    static System CreateSystem(Scene& scene);

    // Plays clips[clip] from the start.
    void Play(int clip, bool loop = true);

    int CurrentClip() const { return m_Clip; }
    const std::shared_ptr<AnimationClip>& GetCurrentClip() const;
    float Time() const { return m_Time; }
    void SetTime(float time) { m_Time = time; }
    bool Loop() const { return m_Loop; }
    void SetLoop(bool loop) { m_Loop = loop; }

    // Advances time by dt (if playing) and poses the bones below `root`.
    void Update(Entity root, float dt);

    // The last pose written, by skeleton bone.
    const Pose& GetPose() const { return m_Pose; }
    // Bones that have an entity below the root, and tracks of the current clip that found a bone.
    size_t BoundBones() const { return m_BoundBones; }
    size_t BoundTracks() const;

private:
    void Bind(Entity root);
    bool NeedsBind() const;
    void WriteBones();

    int m_Clip = -1;
    float m_Time = 0.0f;
    bool m_Loop = true;

    // Found by Bind for the current skeleton and clips (not copied).
    const Skeleton* m_BoundSkeleton = nullptr;
    std::vector<const AnimationClip*> m_BoundClips;
    std::vector<ClipBinding> m_Bindings; // per clip
    std::vector<Entity> m_Bones;         // per bone, invalid where the bone has no entity
    std::vector<uint8_t> m_Channels;     // per bone, which of T/R/S some clip animates
    size_t m_BoundBones = 0;
    Pose m_Pose;
    int m_AppliedClip = -1;
    float m_AppliedTime = -1.0f;
};
