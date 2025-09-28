#pragma once
#include <webgpu/webgpu_cpp.h>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>
#include <vector>
#include <UniformLayout.hpp>
#include <VertexBufferLayout.hpp>
#include <Init.hpp>

class IShader
{
    public:
        virtual ~IShader() = default;

        wgpu::RenderPipeline& GetPipeline() { return m_Pipeline; }
        const wgpu::BindGroup& GetBindGroup(uint32_t index) 
        { 
            if (index == 2)
            {
                Logger::Error("WTF HAPPENED HERE BOI");
            } else if (index == 0)
                return m_UBOBindGroup;
            else if (index == 1)
                return m_TransformBindGroup;
            else if (index == 3)
                return m_CameraBindGroup;
        }

        const wgpu::BindGroupLayout& GetBindGroupLayout(uint32_t index) 
        { 
            if (index == 2)
            {
                Logger::Error("WTF HAPPENED HERE BOI");
            } else if (index == 0)
                return m_UBOBindLayout;
            else if (index == 1)
                return m_TransformBindLayout;
            else if (index == 3)
                return m_CameraBindLayout;
        }

        virtual void LoadShader(std::string shaderCode, std::vector<wgpu::TextureFormat> outputFormats) = 0;

    protected:
        wgpu::RenderPipeline m_Pipeline;
        wgpu::PipelineLayout m_Layout;

        wgpu::BindGroupLayout m_UBOBindLayout{};  
        wgpu::BindGroupLayout m_TransformBindLayout{};  
        wgpu::BindGroupLayout m_MaterialBindLayout{};  
        wgpu::BindGroupLayout m_CameraBindLayout{};
        
        wgpu::BindGroup m_UBOBindGroup{};  
        wgpu::BindGroup m_TransformBindGroup{};  
        wgpu::BindGroup m_CameraBindGroup{};

        uint8_t m_TextureCount;
};

template<typename UBOLayout, typename TransformLayout, typename MaterialLayout, typename CameraLayout>
class Shader : public IShader
{
    public:
        Shader(UniformLayout<UBOLayout> UBOLayout,
               UniformLayout<TransformLayout> transformLayout, 
               UniformLayout<MaterialLayout> materialLayout,
               UniformLayout<CameraLayout> cameraLayout, 
               VertexBufferLayout vertexLayout,
               uint8_t textureCount) : 
               m_UBOLayout(UBOLayout), 
               m_TransformLayout(transformLayout), 
               m_MaterialLayout(materialLayout),
               m_CameraLayout(cameraLayout),
               m_VertexLayout(vertexLayout)
        {
            m_TextureCount = textureCount;
        }
        ~Shader() = default;

        void LoadShader(std::string shaderCode, std::vector<wgpu::TextureFormat> outputFormats) override 
        {
            m_UBOLayout.Init(0);
            m_TransformLayout.Init(0);
            m_MaterialLayout.Init(0);
            m_CameraLayout.Init(0);

            FixBindingLayouts();
            CreateBindgroups();

            wgpu::ShaderSourceWGSL wgsl{{.code = shaderCode.c_str()}};
            wgpu::ShaderModuleDescriptor shaderModuleDescriptor{.nextInChain = &wgsl};
            wgpu::ShaderModule shaderModule =
            device.CreateShaderModule(&shaderModuleDescriptor);

            wgpu::ColorTargetState colorTargetState{.format = outputFormats[0]};
            wgpu::FragmentState fragmentState{
            .module = shaderModule, .targetCount = 1, .targets = &colorTargetState};

            wgpu::DepthStencilState depthStencilState{};
            depthStencilState.depthCompare = wgpu::CompareFunction::Less;
            depthStencilState.depthWriteEnabled = true;
            depthStencilState.format = wgpu::TextureFormat::Depth24Plus;
            depthStencilState.stencilReadMask = 0;
            depthStencilState.stencilWriteMask = 0;

            std::vector<wgpu::BindGroupLayout> bindGroupLayouts = {m_UBOBindLayout, m_TransformBindLayout, m_MaterialBindLayout, m_CameraBindLayout};
            wgpu::PipelineLayoutDescriptor  layoutDesc = {};
            layoutDesc.bindGroupLayoutCount = bindGroupLayouts.size();
            layoutDesc.bindGroupLayouts = bindGroupLayouts.data();
            m_Layout = device.CreatePipelineLayout(&layoutDesc);

            wgpu::RenderPipelineDescriptor descriptor{  .layout = m_Layout,
                                                .vertex = {
                                                  .module = shaderModule,
                                                  .bufferCount = 1,
                                                  .buffers = &m_VertexLayout.vertexBufferLayout
                                                },
                                                .primitive = {
                                                  .stripIndexFormat = wgpu::IndexFormat::Undefined,
                                                  .frontFace = wgpu::FrontFace::CW,
                                                  .cullMode = wgpu::CullMode::Back
                                                },
                                             .depthStencil = &depthStencilState,
                                             .multisample = {
                                                .count = 4,
                                                .mask = ~0u,
                                                .alphaToCoverageEnabled = false
                                             },
                                             .fragment = &fragmentState};

            m_Pipeline = device.CreateRenderPipeline(&descriptor);
        }

        void FixMaterialBindingLayout()
        {
            std::vector<wgpu::BindGroupLayoutEntry> entries;
            entries.resize(m_TextureCount + 2);

            entries[0] = m_MaterialLayout.GetBindGroupLayoutEntry();

            for(size_t i = 1; i < m_TextureCount + 1; i++)
            {
              entries[i] = {};
              entries[i].binding = i;
              entries[i].visibility = wgpu::ShaderStage::Fragment;
              entries[i].texture.sampleType = wgpu::TextureSampleType::Float;
              entries[i].texture.viewDimension = wgpu::TextureViewDimension::e2D;
            }
        
            entries[m_TextureCount + 1] = {};
            entries[m_TextureCount + 1].binding = m_TextureCount + 1;
            entries[m_TextureCount + 1].visibility = wgpu::ShaderStage::Fragment;
            entries[m_TextureCount + 1].sampler.type = wgpu::SamplerBindingType::Filtering;
        
            wgpu::BindGroupLayoutDescriptor textureBindingLayout{};
            textureBindingLayout.entryCount = entries.size();
            textureBindingLayout.entries = entries.data();
            m_MaterialBindLayout = device.CreateBindGroupLayout(&textureBindingLayout);
        }

        void FixBindingLayouts()
        {
            FixMaterialBindingLayout();

            wgpu::BindGroupLayoutDescriptor UBOBindGroupLayoutDesc{};
            UBOBindGroupLayoutDesc.entryCount = 1;
            UBOBindGroupLayoutDesc.entries = &m_UBOLayout.GetBindGroupLayoutEntry();
            m_UBOBindLayout = device.CreateBindGroupLayout(&UBOBindGroupLayoutDesc);

            wgpu::BindGroupLayoutDescriptor TransformBindGroupLayoutDesc{};
            TransformBindGroupLayoutDesc.entryCount = 1;
            TransformBindGroupLayoutDesc.entries = &m_TransformLayout.GetBindGroupLayoutEntry();
            m_TransformBindLayout = device.CreateBindGroupLayout(&TransformBindGroupLayoutDesc);

            wgpu::BindGroupLayoutDescriptor CameraBindGroupLayoutDesc{};
            CameraBindGroupLayoutDesc.entryCount = 1;
            CameraBindGroupLayoutDesc.entries = &m_CameraLayout.GetBindGroupLayoutEntry();
            m_CameraBindLayout = device.CreateBindGroupLayout(&CameraBindGroupLayoutDesc);
        }

        void CreateBindgroups()
        {
            wgpu::BindGroupDescriptor bindGroupDesc{};
            bindGroupDesc.layout = m_UBOBindLayout;
            bindGroupDesc.entryCount = 1;
            bindGroupDesc.entries = &m_UBOLayout.GetBindGroupEntry();
            m_UBOBindGroup = device.CreateBindGroup(&bindGroupDesc);

            bindGroupDesc.layout = m_TransformBindLayout;
            bindGroupDesc.entryCount = 1;
            bindGroupDesc.entries = &m_TransformLayout.GetBindGroupEntry();
            m_TransformBindGroup = device.CreateBindGroup(&bindGroupDesc);

            bindGroupDesc.layout = m_CameraBindLayout;
            bindGroupDesc.entryCount = 1;
            bindGroupDesc.entries = &m_CameraLayout.GetBindGroupEntry();
            m_CameraBindGroup = device.CreateBindGroup(&bindGroupDesc);
        }

    public:
        UniformLayout<UBOLayout> m_UBOLayout;
        UniformLayout<TransformLayout> m_TransformLayout;
        UniformLayout<MaterialLayout> m_MaterialLayout;
        UniformLayout<CameraLayout> m_CameraLayout;
        VertexBufferLayout m_VertexLayout;
};