#pragma once
#include <webgpu/webgpu_cpp.h>
#include <string>
#include <unordered_map>

#include <Buffers.hpp>

class Shader
{
    public:
        Shader();
        ~Shader();

        void LoadShader(std::string shaderCode);

        wgpu::RenderPipeline& GetPipeline() { return m_Pipeline; }

    private:
        wgpu::RenderPipeline m_Pipeline;
        Buffers m_Buffers;
};