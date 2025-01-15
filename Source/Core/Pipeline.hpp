#pragma once
#include <webgpu/webgpu_cpp.h>

class Pipeline
{
    public:
        Pipeline(const char* shaderName);
        ~Pipeline();

    private:
        wgpu::ShaderModule shaderModule;
};