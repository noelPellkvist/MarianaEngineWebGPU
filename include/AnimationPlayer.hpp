#pragma once
#include <ECS.hpp>
#include <Animation.hpp>
#include <vector>
#include <AssetManager.hpp>

struct AnimationPlayer
{
    float m_Time = 0;
    Animation* m_CurrentAnimation;
    std::vector<int> prevKeyframes;
    Entity m_Root;
    struct Tmp { int translationId = -1; int rotationId = -1; int scaleId = -1; };
    std::vector<Tmp> nodesData;
    

    void SetAnimation(Animation* animation);
    void SetEntityRoot(Entity root);
    void UpdateTime(float dt);
    void UpdateEntity(Entity e, AnimationTargetEntity target);
    void InternalSetup(Entity e);
    std::vector<float> GetCurrentDataVec(int currentIndex, int channelIndex);
    std::vector<float> GetCurrentDataQuat(int currentIndex, int channelIndex);
};