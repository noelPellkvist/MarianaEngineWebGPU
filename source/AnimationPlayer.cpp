#include <AnimationPlayer.hpp>
#include <Logger.hpp>
#include <AssetManager.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>

void AnimationPlayer::SetEntityRoot(Entity root)
{
    m_Root = root;
}

void AnimationPlayer::InternalSetup(Entity e)
{
    if (!e.IsValid())
        return;

    if (e.Has<AnimationTargetEntity>())
    {
        AnimationTargetEntity& target = *e.Get<AnimationTargetEntity>();

        target.translationTargetChannel = nodesData[target.nodeIndex].translationId;
        target.rotationTargetChannel = nodesData[target.nodeIndex].rotationId;
        target.scaleTargetCHannel = nodesData[target.nodeIndex].scaleId;
    }

    e.ForEachChild([&](Entity child){
        InternalSetup(child);
    });
}

void AnimationPlayer::SetAnimation(Animation* animation)
{
    m_Time = 0;
    m_CurrentAnimation = animation;
    prevKeyframes.clear();
    prevKeyframes.reserve(animation->GetChannelCount());

    if (!m_Root.IsValid())
    {
        Logger::Error("SetEntityRoot before SetAnimation so AnimationPlayer has a valid root");
        return;
    }

    nodesData.clear();
    
    nodesData.resize(animation->GetNodeCount());

    for (size_t i = 0; i < animation->GetChannelCount(); ++i)
    {
        prevKeyframes.push_back(0);
        switch (animation->GetChannel(i).type)
        {
        case AnimationChannelType::Translation:
            nodesData[animation->GetChannels()[i].targetNodeIndex].translationId = i;
            break;
        
        case AnimationChannelType::Rotation:
            nodesData[animation->GetChannels()[i].targetNodeIndex].rotationId = i;
            break;
        
        case AnimationChannelType::Scale:
            nodesData[animation->GetChannels()[i].targetNodeIndex].scaleId = i;
            break;
        
        default:
            Logger::Error("Non-standard animation channels are not supported bro");
            break;
        }
    }
    InternalSetup(m_Root);
}

void AnimationPlayer::UpdateTime(float dt)
{
    m_Time += (1.0f / 200.0f);
}

void AnimationPlayer::UpdateEntity(Entity e, AnimationTargetEntity target)
{
    int i = (static_cast<int>(m_Time / 3)) % 10;
    if (target.translationTargetChannel != -1)
    {
        auto pos = m_CurrentAnimation->GetChannel(target.translationTargetChannel).keyFrames[i].data;
        e.SetPosition(pos[0], pos[1], pos[2]);
    }
    if (target.rotationTargetChannel != -1)
    {
        auto q = m_CurrentAnimation->GetChannel(target.rotationTargetChannel).keyFrames[i].data;
        glm::quat quat(q[3], q[0], q[1], q[2]); 
        glm::vec3 euler = glm::eulerAngles(quat);
        e.SetRotationEuler(euler.x, euler.y, euler.z);
    }
    if (target.scaleTargetCHannel != -1)
    {
        auto scale = m_CurrentAnimation->GetChannel(target.scaleTargetCHannel).keyFrames[i].data;
        e.SetScale(scale[0], scale[1], scale[2]);
    }
}