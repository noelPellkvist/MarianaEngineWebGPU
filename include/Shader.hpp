#pragma once
#include <webgpu/webgpu_cpp.h>
#include <string>
#include <unordered_map>

class Shader
{
    public:
        Shader();
        ~Shader();

        void LoadShader(std::string shaderCode, std::vector<wgpu::TextureFormat> outputFormats);

        wgpu::RenderPipeline& GetPipeline() { return m_Pipeline; }

    private:
        wgpu::RenderPipeline m_Pipeline;
};