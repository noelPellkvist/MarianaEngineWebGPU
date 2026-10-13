#include <Mariana/Animation/Animation.hpp>

Animation& Animation::Build()
{
    duration = 0;
    for (AnimationChannel& c : channels)
    {
        if (!c.keyFrames.empty() && c.keyFrames.back().time > duration) duration = c.keyFrames.back().time;
    }
    return *this;
}
