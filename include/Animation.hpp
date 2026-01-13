#pragma once
#include <vector>
#include <string>


enum class AnimationChannelType { Translation, Rotation, Scale, Weights };
enum class AnimationInterpolationType { Step, Linear, CubicSpline };

struct AnimationKeyFrame
{
    float time; //enfore size of these somewhere?
    std::vector<float> data;
    std::vector<float> inTangent;
    std::vector<float> outTangent;
};

struct AnimationChannel
{
    AnimationChannelType type;
    AnimationInterpolationType interpolation;
    int targetNodeIndex;
    std::vector<AnimationKeyFrame> keyFrames;
};

class Animation
{
public:
    Animation(const std::string& name) : name(name) {}

    const std::string& GetName() const { return name; }
    float GetDuration() const { return duration; }

    size_t GetChannelCount() const { return channels.size(); }
    const AnimationChannel& GetChannel(size_t i) const { return channels[i]; }
    AnimationChannel& GetChannel(size_t i) { return channels[i]; }

    void SetNodeCount(int nodes) { maxNodes = nodes; }
    const int GetNodeCount() const { return maxNodes; }

    AnimationChannel& AddChannel(const AnimationChannel& channel)
    {
        channels.push_back(channel);
        return channels.back();
    }

    const std::vector<AnimationChannel>& GetChannels() const { return channels; };

    Animation& Build();

private:
    std::string name;
    float duration = 0;
    int maxNodes;
    std::vector<AnimationChannel> channels;
};