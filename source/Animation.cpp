#include <Animation.hpp>

Animation& Animation::Build()
{
    for (AnimationChannel& c : channels)
    {
        if (c.keyFrames.back().time > duration) duration = c.keyFrames.back().time;
    }
    return *this;
}