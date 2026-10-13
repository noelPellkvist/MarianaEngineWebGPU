#pragma once
#include <Mariana/Scene/ECS.hpp>
#include <Mariana/Animation/Animation.hpp>
#include <Mariana/Assets/AssetManager.hpp>
#include <memory>
#include <vector>

// Marks an entity as glTF node `nodeIndex`, so animation channels targeting that node find it.
struct AnimationTargetEntity
{
    uint32_t nodeIndex = 0;
};

// Plays one of `clips` on the entity it's attached to and its descendants. Descendants that
// have their own AnimationPlayer (a nested model) are left to that player.
struct AnimationPlayer
{
    std::vector<AssetHandle<Animation>> clips;
    int currentClip = -1;
    float time = 0.0f;
    float speed = 1.0f;
    bool playing = true;
    bool loop = true;

    AnimationPlayer() = default;
    // Copies only the playback settings; the bound targets belong to the source's entities.
    AnimationPlayer(const AnimationPlayer& other);
    AnimationPlayer& operator=(const AnimationPlayer& other);
    AnimationPlayer(AnimationPlayer&&) = default;
    AnimationPlayer& operator=(AnimationPlayer&&) = default;

    // AnimationPlayer and AnimationTargetEntity; call before Scene::UpdateComponentRegistry.
    static void RegisterComponents();

    // Switches to clips[clipIndex] and plays it from the start.
    void Play(int clipIndex);

    // Advances time by dt (if playing) and poses the hierarchy below `root`.
    void Update(Entity root, float dt);

    const std::shared_ptr<Animation>& GetCurrentClip() const;

    // Cached for the bound clip: every animated entity with the channel driving its
    // translation/rotation/scale (-1 = none), and a keyframe cursor per channel so sampling
    // stays cheap while time advances.
    struct Target { Entity entity; int translation = -1; int rotation = -1; int scale = -1; };
    std::vector<Target> targets;
    std::vector<size_t> cursors;
    int boundClip = -1;
    float appliedTime = -1.0f;

private:
    void Bind(Entity root, const Animation& clip);
    void ApplyToTarget(const Target& target, const Animation& clip);
};
