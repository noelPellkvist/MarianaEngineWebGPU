#pragma once
#include <webgpu/webgpu_cpp.h>

class Pipeline
{
    public:
        Pipeline(const char* shaderName, wgpu::TextureFormat format, wgpu::Buffer* ubo, wgpu::Sampler* sampler);
        ~Pipeline();

        wgpu::TextureView depthTextureView;
        wgpu::RenderPipeline pipeline;
        wgpu::BindGroup uboBindGroup;

        void InitDepthTexture();

    private:
        std::vector<wgpu::VertexAttribute> attributes;
        std::vector<wgpu::VertexAttribute> skinnedVertexAttributes;
        std::vector<wgpu::VertexBufferLayout> vertexBufferLayouts;
        std::vector<wgpu::BindGroupLayout> bindgroupLayouts;

        wgpu::Buffer* UboBuffer;
        wgpu::Sampler* sampler;
        wgpu::TextureFormat format;
        

        
        wgpu::PipelineLayout layout;
        

        void PopulateVertexBufferLayouts();
        void PopulateGlobalBindings();

        
};