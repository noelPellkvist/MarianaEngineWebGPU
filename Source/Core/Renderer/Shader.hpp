#pragma once
#include <vector>
#include <string>
#include <webgpu/webgpu_cpp.h>

class Shader
{
    public:
        Shader(std::string shaderName, wgpu::TextureFormat targetFormat);
        ~Shader();

        wgpu::RenderPipeline& GetRenderPipeline() { return m_Pipeline; };

        wgpu::Buffer vertexBuffer;

    private:
        wgpu::RenderPipeline m_Pipeline;

        void CreateRenderPipeline(wgpu::TextureFormat targetFormat);



        
};