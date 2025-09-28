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
        const wgpu::BindGroup& GetUBOBindGroup() const { return m_UBOBindGroup; };
        const wgpu::BindGroup& GetPerDrawBindGroup() const { return m_PerDrawBindGroup; };
        const wgpu::BindGroup& GetCameraBindgroup() const { return m_CameraBindGroup; };

        virtual void LoadShader(std::string shaderCode, std::vector<wgpu::TextureFormat> outputFormats) = 0;

    protected:
        wgpu::RenderPipeline m_Pipeline;
        wgpu::PipelineLayout m_Layout;

        wgpu::BindGroupLayout m_UBOBindLayout{};  
        wgpu::BindGroupLayout m_PerDrawBindLayout{};  
        wgpu::BindGroupLayout m_CameraBindLayout{};
        
        wgpu::BindGroup m_UBOBindGroup{};  
        wgpu::BindGroup m_PerDrawBindGroup{};  
        wgpu::BindGroup m_CameraBindGroup{};

        uint8_t m_TextureCount;

        void FixTextureBindings();
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
            FixTextureBindings();
            m_UBOLayout.Init();
            m_TransformLayout.Init();
            m_MaterialLayout.Init();
            m_CameraLayout.Init();

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

            std::vector<wgpu::BindGroupLayout> bindGroupLayouts = {m_UBOBindLayout, m_PerDrawBindLayout, m_CameraBindLayout};
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

    private:
        UniformLayout<UBOLayout> m_UBOLayout;
        UniformLayout<TransformLayout> m_TransformLayout;
        UniformLayout<MaterialLayout> m_MaterialLayout;
        UniformLayout<CameraLayout> m_CameraLayout;
        VertexBufferLayout m_VertexLayout;

        
};