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
        static wgpu::Sampler CreateSampler(int minFilter, int magFilter, WrapMode wrapS, WrapMode wrapT);
};