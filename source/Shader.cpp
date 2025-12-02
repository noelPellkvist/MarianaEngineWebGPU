#include <Shader.hpp>
#include <Logger.hpp>
#include "Init.hpp"
#include <webgpu/webgpu_cpp.h>

struct IShader::Impl
{
    wgpu::RenderPipeline m_Pipeline;
    wgpu::PipelineLayout m_Layout;

    std::vector<wgpu::BindGroupLayout> m_BindgroupLayouts;
    std::vector<wgpu::BindGroup> m_Bindgroups;  
};

IShader::IShader(VertexBufferLayout vbl, std::vector<TextureType> textureTypes, const Renderpass& renderpass)
    : _impl(std::make_unique<Impl>()),
      m_TextureTypes(textureTypes),
      m_VertexLayout(std::move(vbl)),
      m_Renderpass(renderpass) {
        m_TextureCount = static_cast<uint16_t>(textureTypes.size());
      }

IShader::~IShader() = default;

void* IShader::GetBindGroup(uint32_t index) 
{ 
    if (index == GetBindingsCount() - 1)
    {
        Logger::Error("WHY YOU CALLING THIS???");
    } else
        return &_impl->m_Bindgroups[index];
}

void* IShader::GetBindGroupLayout(uint32_t index) 
{ 
    return &_impl->m_BindgroupLayouts[index];
}

void* IShader::GetPipeline() 
{ 
    return &_impl->m_Pipeline; 
}

void IShader::LoadShader(std::string shaderCode) 
{
    _impl->m_BindgroupLayouts.resize(GetBindingsCount());
    _impl->m_Bindgroups.resize(GetBindingsCount());
    InitBuffers();
    wgpu::ShaderSourceWGSL wgsl{{.code = shaderCode.c_str()}};
    wgpu::ShaderModuleDescriptor shaderModuleDescriptor{.nextInChain = &wgsl};

    wgpu::ShaderModule shaderModule =
    device.CreateShaderModule(&shaderModuleDescriptor);
    const std::vector<TextureFormat>& outputFormats = m_Renderpass.GetOutputFormats();
    std::vector<wgpu::ColorTargetState> colorTargetStates(outputFormats.size());

    for(size_t i = 0; i < outputFormats.size(); i++)
    {
        colorTargetStates[i] = {
            .format = static_cast<wgpu::TextureFormat>((uint32_t)(outputFormats[i])),
            .writeMask = wgpu::ColorWriteMask::All,
        };
    }

    wgpu::FragmentState fragmentState{
    .module = shaderModule, .entryPoint = wgpu::StringView("fragmentMain"), .targetCount = 2, .targets = colorTargetStates.data()};

    wgpu::DepthStencilState depthStencilState{};
    depthStencilState.depthCompare = wgpu::CompareFunction::LessEqual;
    depthStencilState.depthWriteEnabled = true;
    depthStencilState.format = wgpu::TextureFormat::Depth24Plus;
    depthStencilState.stencilReadMask = 0;
    depthStencilState.stencilWriteMask = 0;
    
    wgpu::PipelineLayoutDescriptor  layoutDesc = {};
    layoutDesc.bindGroupLayoutCount = _impl->m_BindgroupLayouts.size();
    layoutDesc.bindGroupLayouts = _impl->m_BindgroupLayouts.data();
    _impl->m_Layout = device.CreatePipelineLayout(&layoutDesc);

    wgpu::RenderPipelineDescriptor descriptor{  .layout = _impl->m_Layout,
                                        .vertex = {
                                          .module = shaderModule,
                                          .entryPoint = wgpu::StringView("vertexMain"),
                                          .bufferCount = 1,
                                          .buffers = static_cast<const wgpu::VertexBufferLayout*>(m_VertexLayout.GetBackendLayout())
                                        },
                                        .primitive = {
                                          .stripIndexFormat = wgpu::IndexFormat::Undefined,
                                          .frontFace = wgpu::FrontFace::CW,
                                          .cullMode = wgpu::CullMode::Back
                                        },
                                     .depthStencil = &depthStencilState,
                                     .multisample = {
                                        .count = 1,
                                        .mask = ~0u,
                                        .alphaToCoverageEnabled = false
                                     },
                                     .fragment = &fragmentState};

   
    
    _impl->m_Pipeline = device.CreateRenderPipeline(&descriptor);
}

void IShader::FixMaterialBindingLayout()
{
    std::vector<wgpu::BindGroupLayoutEntry> entries;
    entries.resize(m_TextureCount + 2);

    entries[0] = *static_cast<wgpu::BindGroupLayoutEntry*>(GetBindGroupLayoutEntry(GetBindingsCount() - 1));

    for(size_t i = 1; i < m_TextureCount + 1; i++)
    {
      entries[i] = {};
      entries[i].binding = i;
      entries[i].visibility = wgpu::ShaderStage::Fragment;
      entries[i].texture.sampleType = wgpu::TextureSampleType::Float;

      if(m_TextureTypes[i - 1] == TextureType_Cube)
          entries[i].texture.viewDimension = wgpu::TextureViewDimension::Cube;
      else if (m_TextureTypes[i - 1] == TextureType_2D)
        entries[i].texture.viewDimension = wgpu::TextureViewDimension::e2D;
      else
          Logger::Error("Unsupported texture type in shader!");
    }

    entries[m_TextureCount + 1] = {};
    entries[m_TextureCount + 1].binding = m_TextureCount + 1;
    entries[m_TextureCount + 1].visibility = wgpu::ShaderStage::Fragment;
    entries[m_TextureCount + 1].sampler.type = wgpu::SamplerBindingType::Filtering;

    wgpu::BindGroupLayoutDescriptor textureBindingLayout{};
    textureBindingLayout.label = "Material Bindgroup Layout";
    textureBindingLayout.entryCount = entries.size();
    textureBindingLayout.entries = entries.data();
    auto m = GetBindingsCount();
    _impl->m_BindgroupLayouts[GetBindingsCount() - 1] = device.CreateBindGroupLayout(&textureBindingLayout);
}

void IShader::FixBindingLayouts()
{
    FixMaterialBindingLayout();

    for(size_t i = 0; i < GetBindingsCount() - 1; i++)
    {
        std::string label = "Bindgroup Layout " + std::to_string(i);
        wgpu::BindGroupLayoutDescriptor BindGroupLayoutDesc{};
        BindGroupLayoutDesc.label = label.c_str();
        BindGroupLayoutDesc.entryCount = 1;
        BindGroupLayoutDesc.entries = static_cast<wgpu::BindGroupLayoutEntry*>(GetBindGroupLayoutEntry(i));
        _impl->m_BindgroupLayouts[i] = device.CreateBindGroupLayout(&BindGroupLayoutDesc);
    }
}

void IShader::CreateBindgroups()
{

    for (size_t i = 0; i < GetBindingsCount() - 1; i++)
    {
        wgpu::BindGroupDescriptor bindGroupDesc{};
        bindGroupDesc.layout = _impl->m_BindgroupLayouts[i];
        bindGroupDesc.entryCount = 1;
        bindGroupDesc.entries = static_cast<wgpu::BindGroupEntry*>(GetBindGroupEntry(i));
        _impl->m_Bindgroups[i] = device.CreateBindGroup(&bindGroupDesc);
    }
}