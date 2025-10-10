#include <Shader.hpp>
#include "Init.hpp"
#include <webgpu/webgpu_cpp.h>

struct IShader::Impl
{
    wgpu::RenderPipeline m_Pipeline;
    wgpu::PipelineLayout m_Layout;

    wgpu::BindGroupLayout m_UBOBindLayout{};  
    wgpu::BindGroupLayout m_TransformBindLayout{};  
    wgpu::BindGroupLayout m_MaterialBindLayout{};  
    wgpu::BindGroupLayout m_CameraBindLayout{};
        
    wgpu::BindGroup m_UBOBindGroup{};  
    wgpu::BindGroup m_TransformBindGroup{};  
    wgpu::BindGroup m_CameraBindGroup{};    
};

IShader::IShader(VertexBufferLayout vbl, uint8_t textureCount, const Renderpass& renderpass)
    : _impl(std::make_unique<Impl>()),
      m_TextureCount(textureCount),
      m_VertexLayout(std::move(vbl)),
      m_Renderpass(renderpass) {}

IShader::~IShader() = default;

void* IShader::GetBindGroup(uint32_t index) 
{ 
    if (index == 2)
    {
        Logger::Error("WHY YOU CALLING THIS???");
    } else if (index == 0)
        return &_impl->m_UBOBindGroup;
    else if (index == 1)
        return &_impl->m_TransformBindGroup;
    else if (index == 3)
        return &_impl->m_CameraBindGroup;
}

void* IShader::GetBindGroupLayout(uint32_t index) 
{ 
    if (index == 2)
    {
        return &_impl->m_MaterialBindLayout;
    } else if (index == 0)
        return &_impl->m_UBOBindLayout;
    else if (index == 1)
        return &_impl->m_TransformBindLayout;
    else if (index == 3)
        return &_impl->m_CameraBindLayout;
}

void* IShader::GetPipeline() 
{ 
    return &_impl->m_Pipeline; 
}

void IShader::LoadShader(std::string shaderCode) 
{
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
    depthStencilState.depthCompare = wgpu::CompareFunction::Less;
    depthStencilState.depthWriteEnabled = true;
    depthStencilState.format = wgpu::TextureFormat::Depth24Plus;
    depthStencilState.stencilReadMask = 0;
    depthStencilState.stencilWriteMask = 0;

    std::vector<wgpu::BindGroupLayout> bindGroupLayouts = {_impl->m_UBOBindLayout, _impl->m_TransformBindLayout, _impl->m_MaterialBindLayout, _impl->m_CameraBindLayout};
    
    wgpu::PipelineLayoutDescriptor  layoutDesc = {};
    layoutDesc.bindGroupLayoutCount = bindGroupLayouts.size();
    layoutDesc.bindGroupLayouts = bindGroupLayouts.data();
    _impl->m_Layout = device.CreatePipelineLayout(&layoutDesc);

    wgpu::RenderPipelineDescriptor descriptor{  .layout = _impl->m_Layout,
                                        .vertex = {
                                          .module = shaderModule,
                                          .entryPoint = wgpu::StringView("vertexMain"),
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

   
    
    _impl->m_Pipeline = device.CreateRenderPipeline(&descriptor);
}

void IShader::FixMaterialBindingLayout()
{
    std::vector<wgpu::BindGroupLayoutEntry> entries;
    entries.resize(m_TextureCount + 2);

    entries[0] = *static_cast<wgpu::BindGroupLayoutEntry*>(GetMaterialBindGroupLayoutEntry());

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
    _impl->m_MaterialBindLayout = device.CreateBindGroupLayout(&textureBindingLayout);
}

void IShader::FixBindingLayouts()
{
    FixMaterialBindingLayout();

    wgpu::BindGroupLayoutDescriptor UBOBindGroupLayoutDesc{};
    UBOBindGroupLayoutDesc.entryCount = 1;
    UBOBindGroupLayoutDesc.entries = static_cast<wgpu::BindGroupLayoutEntry*>(GetUBOBindGroupLayoutEntry());
    _impl->m_UBOBindLayout = device.CreateBindGroupLayout(&UBOBindGroupLayoutDesc);

    wgpu::BindGroupLayoutDescriptor TransformBindGroupLayoutDesc{};
    TransformBindGroupLayoutDesc.entryCount = 1;
    TransformBindGroupLayoutDesc.entries = static_cast<wgpu::BindGroupLayoutEntry*>(GetTransformBindGroupLayoutEntry());
    _impl->m_TransformBindLayout = device.CreateBindGroupLayout(&TransformBindGroupLayoutDesc);

    wgpu::BindGroupLayoutDescriptor CameraBindGroupLayoutDesc{};
    CameraBindGroupLayoutDesc.entryCount = 1;
    CameraBindGroupLayoutDesc.entries = static_cast<wgpu::BindGroupLayoutEntry*>(GetCameraBindGroupLayoutEntry());
    _impl->m_CameraBindLayout = device.CreateBindGroupLayout(&CameraBindGroupLayoutDesc);
}

void IShader::CreateBindgroups()
{
    wgpu::BindGroupDescriptor bindGroupDesc{};
    bindGroupDesc.layout = _impl->m_UBOBindLayout;
    bindGroupDesc.entryCount = 1;
    bindGroupDesc.entries = static_cast<wgpu::BindGroupEntry*>(GetUBOBindGroupEntry());
    _impl->m_UBOBindGroup = device.CreateBindGroup(&bindGroupDesc);

    bindGroupDesc.layout = _impl->m_TransformBindLayout;
    bindGroupDesc.entryCount = 1;
    bindGroupDesc.entries = static_cast<wgpu::BindGroupEntry*>(GetTransformBindGroupEntry());
    _impl->m_TransformBindGroup = device.CreateBindGroup(&bindGroupDesc);

    bindGroupDesc.layout = _impl->m_CameraBindLayout;
    bindGroupDesc.entryCount = 1;
    bindGroupDesc.entries = static_cast<wgpu::BindGroupEntry*>(GetCameraBindGroupEntry());
    _impl->m_CameraBindGroup = device.CreateBindGroup(&bindGroupDesc);
}