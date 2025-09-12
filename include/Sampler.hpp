#pragma once
#include <webgpu/webgpu_cpp.h>

enum WrapMode
{
    REPEAT = 0,
    CLAMP_TO_EDGE = 1,
    MIRRORED_REPEAT = 2
};

class Sampler
{
    public:
        int minFilter = -1;
        int magFilter = -1;
        WrapMode wrapS = REPEAT;
        WrapMode wrapT = REPEAT;

        Sampler() = default;
        Sampler(int minFilter, int magFilter, WrapMode wrapS, WrapMode wrap) : 
            minFilter(minFilter), magFilter(magFilter), wrapS(wrapS), wrapT(wrap) {}

        wgpu::Sampler CreateSampler();
};