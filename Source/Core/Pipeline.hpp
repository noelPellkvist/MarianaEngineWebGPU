#pragma once
#include <webgpu/webgpu_cpp.h>
#include <vector>

enum BindingType
{
    BUILT_IN_UBO,
    BUILT_IN_MODELDATA,
    BUILT_IN_BONES,
    e2D,
    e3D,
    eCube
};

struct PipelineCreateInfo
{
    std::vector<BindingType> bindings;
};

class Pipeline
{        

    public:
        Pipeline(const char* shaderName, wgpu::TextureFormat format, wgpu::Buffer* ubo, wgpu::Sampler* sampler, const std::vector<BindingType>& bindings);
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
        std::vector<wgpu::BindGroupLayoutEntry> textureBindingLayouts;
        std::vector<BindingType> bindings;

        wgpu::Buffer* UboBuffer;
        wgpu::Sampler* sampler;
        wgpu::TextureFormat format;
        

        
        wgpu::PipelineLayout layout;
        

        void PopulateVertexBufferLayouts();
        void PopulateGlobalBindings();
        void PopulateModelBindings();
        void PopulateBoneBindings();
        void BindExtraTexture(BindingType type);
        void CreatePipelineLayout();

        
};