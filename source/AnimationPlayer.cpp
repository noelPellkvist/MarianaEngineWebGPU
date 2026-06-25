#include <AnimationPlayer.hpp>
#include <Logger.hpp>
#include <AssetManager.hpp>

#include <algorithm>

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
    m_Time += (0.6f / 60.0f);
    auto duration = m_CurrentAnimation->GetDuration();
    if (m_Time > m_CurrentAnimation->GetDuration())
    {
        for (int& i : prevKeyframes)
            i = 0;
        m_Time = 0;
    }
}

std::vector<float> AnimationPlayer::GetCurrentDataVec(int currentIndex, int channelIndex) 
{
    AnimationChannel& channel = m_CurrentAnimation->GetChannel(channelIndex);
    std::vector<float> ret(3, 0);

    if (currentIndex == channel.keyFrames.size() - 1) 
        return channel.keyFrames.back().data;

    if (currentIndex == 0 && (m_Time <= channel.keyFrames[0].time)) 
        return channel.keyFrames[0].data;

    if (channel.keyFrames[currentIndex + 1].time < m_Time)
    {
        currentIndex++;
        prevKeyframes[channelIndex]++;
    }

    float interpolationStart = channel.keyFrames[currentIndex].time;
    float interpolationEnd = channel.keyFrames[currentIndex + 1].time;
    float keyframeDuration = interpolationEnd - interpolationStart;
    float interpolation = (m_Time - interpolationStart) / keyframeDuration;
    interpolation = std::clamp(interpolation, 0.0f, 1.0f);

    switch (channel.interpolation)
    {
    case AnimationInterpolationType::Step:
        ret = channel.keyFrames[currentIndex].data;
        break;

    case AnimationInterpolationType::Linear:
        ret[0] = (1 - interpolation) * channel.keyFrames[currentIndex].data[0] + interpolation * channel.keyFrames[currentIndex + 1].data[0];
        ret[1] = (1 - interpolation) * channel.keyFrames[currentIndex].data[1] + interpolation * channel.keyFrames[currentIndex + 1].data[1];
        ret[2] = (1 - interpolation) * channel.keyFrames[currentIndex].data[2] + interpolation * channel.keyFrames[currentIndex + 1].data[2];
    break;

    case AnimationInterpolationType::CubicSpline:
    {
        float t  = interpolation;
        float t2 = t * t;
        float t3 = t2 * t;

        float h00 =  2.0f * t3 - 3.0f * t2 + 1.0f;
        float h10 =        t3 - 2.0f * t2 + t;
        float h01 = -2.0f * t3 + 3.0f * t2;
        float h11 =        t3 -        t2;

        const AnimationKeyFrame& kf0 = channel.keyFrames[currentIndex];
        const AnimationKeyFrame& kf1 = channel.keyFrames[currentIndex + 1];

        for (int i = 0; i < 3; ++i)
        {
            float p0 = kf0.data[i];
            float p1 = kf1.data[i];
        
            float m0 = kf0.outTangent[i] * keyframeDuration;
            float m1 = kf1.inTangent[i]  * keyframeDuration;

            ret[i] =
                h00 * p0 +
                h10 * m0 +
                h01 * p1 +
                h11 * m1;
        }
    }
    break;

    
    default:
        break;
    }

    return ret;
}

std::vector<float> AnimationPlayer::GetCurrentDataQuat(int currentIndex, int channelIndex) 
{
    AnimationChannel& channel = m_CurrentAnimation->GetChannel(channelIndex);
    std::vector<float> ret(4, 0);

    if (currentIndex == channel.keyFrames.size() - 1) 
        return channel.keyFrames.back().data;

    if (currentIndex == 0 && (m_Time <= channel.keyFrames[0].time)) 
        return channel.keyFrames[0].data;

    if (channel.keyFrames[currentIndex + 1].time < m_Time)
    {
        currentIndex++;
        prevKeyframes[channelIndex]++;
    }

    if (currentIndex == channel.keyFrames.size() - 1) 
        return channel.keyFrames.back().data;

    float interpolationStart = channel.keyFrames[currentIndex].time;
    float interpolationEnd = channel.keyFrames[currentIndex + 1].time;
    float keyframeDuration = interpolationEnd - interpolationStart;
    float interpolation = (m_Time - interpolationStart) / keyframeDuration;
    interpolation = std::clamp(interpolation, 0.0f, 1.0f);

    ret = channel.keyFrames[currentIndex].data;

    // switch (channel.interpolation)
    // {
    // case AnimationInterpolationType::Step:
    //     ret = channel.keyFrames[currentIndex].data;
    //     break;

    // case AnimationInterpolationType::Linear:
    //     ret[0] = (1 - interpolation) * channel.keyFrames[currentIndex].data[0] + interpolation * channel.keyFrames[currentIndex + 1].data[0];
    //     ret[1] = (1 - interpolation) * channel.keyFrames[currentIndex].data[1] + interpolation * channel.keyFrames[currentIndex + 1].data[1];
    //     ret[2] = (1 - interpolation) * channel.keyFrames[currentIndex].data[2] + interpolation * channel.keyFrames[currentIndex + 1].data[2];
    // break;

    // case AnimationInterpolationType::CubicSpline:
    // {
    //     float t  = interpolation;
    //     float t2 = t * t;
    //     float t3 = t2 * t;

    //     float h00 =  2.0f * t3 - 3.0f * t2 + 1.0f;
    //     float h10 =        t3 - 2.0f * t2 + t;
    //     float h01 = -2.0f * t3 + 3.0f * t2;
    //     float h11 =        t3 -        t2;

    //     const AnimationKeyFrame& kf0 = channel.keyFrames[currentIndex];
    //     const AnimationKeyFrame& kf1 = channel.keyFrames[currentIndex + 1];

    //     for (int i = 0; i < 3; ++i)
    //     {
    //         float p0 = kf0.data[i];
    //         float p1 = kf1.data[i];
        
    //         float m0 = kf0.outTangent[i] * keyframeDuration;
    //         float m1 = kf1.inTangent[i]  * keyframeDuration;

    //         ret[i] =
    //             h00 * p0 +
    //             h10 * m0 +
    //             h01 * p1 +
    //             h11 * m1;
    //     }
    // }
    // break;

    
    // default:
    //     break;
    // }

    return ret;
}

void AnimationPlayer::UpdateEntity(Entity e, AnimationTargetEntity target)
{
    auto namn = e.GetName();
    int i = (static_cast<int>(m_Time / 3)) % 10;
    if (target.translationTargetChannel != -1)
    {
        auto pos = GetCurrentDataVec(prevKeyframes[target.translationTargetChannel], target.translationTargetChannel);
        e.SetPosition(pos[0], pos[1], pos[2]);
    }
    if (target.rotationTargetChannel != -1)
    {
        auto rot = GetCurrentDataQuat(prevKeyframes[target.rotationTargetChannel], target.rotationTargetChannel);
        glm::quat q(rot[3], rot[0], rot[1], rot[2]);
        e.SetRotationQuat(q.x, q.y, q.z, q.w);
    }
    if (target.scaleTargetCHannel != -1)
    {
        auto scale = GetCurrentDataVec(prevKeyframes[target.scaleTargetCHannel], target.scaleTargetCHannel);
        e.SetScale(scale[0], scale[1], scale[2]);
    }
}