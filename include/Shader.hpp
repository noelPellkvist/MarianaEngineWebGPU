#pragma once
#include <webgpu/webgpu_cpp.h>
#include <string>

class Shader
{
    public:
        Shader();
        ~Shader();

        void LoadShader(std::string shaderCode);

    private:
        wgpu::RenderPipeline pipeline;
};