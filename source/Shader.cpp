#include <Shader.hpp>
#include <Logger.hpp>
#include "Init.hpp"
#include <webgpu/webgpu_cpp.h>
#include <Buffer.hpp>

struct SamplerResource::Impl
{
    wgpu::Sampler m_Sampler;
};

struct BindGroup::Impl
{
    wgpu::BindGroup m_Bindgroup;
};

struct Shader2::Impl
{
    wgpu::RenderPipeline m_Pipeline;
    wgpu::PipelineLayout m_Layout;

    std::vector<wgpu::BindGroupLayout> m_BindgroupLayouts;
};

BufferResource::~BufferResource() = default;    
BufferResource::BufferResource(const char* name, uint32_t binding, Buffer& buffer)
    : name(name),
      binding(binding),
      buffer(&buffer) {}

TextureResource::~TextureResource() = default;
SamplerResource::~SamplerResource() = default;
SamplerResource::SamplerResource(const char* name, uint32_t binding, bool isComparison)
    : name(name),
      binding(binding),
        isComparison(isComparison),
      _impl(std::make_shared<Impl>()) {}

BindGroup::BindGroup() : _impl(std::make_shared<Impl>()) {}

BindGroup& BindGroup::AddBuffer(const char* name, uint32_t binding, Buffer& buffer) {
    resources.emplace_back(BufferResource{ name, binding, buffer });
    if (buffer.IsDynamic())
        m_DynamicBufferCount++;
    return *this;
}

BindGroup& BindGroup::AddTexture(const char* name, uint32_t binding, TextureType type) {
    resources.emplace_back(TextureResource{ name, binding, type });
    return *this;
}

BindGroup& BindGroup::AddSampler(const char* name, uint32_t binding, bool isComparison) {
    resources.emplace_back(SamplerResource{ name, binding, isComparison });
    return *this;
}

void* BindGroup::GetBindGroup()
{
    return &_impl->m_Bindgroup;
}


#pragma region Bindgroup Layout Entry Builders

wgpu::BindGroupLayoutEntry BuildTextureLayoutEntry(uint32_t binding, TextureType type)
{
    wgpu::BindGroupLayoutEntry entry{};
    entry.binding = binding;
    entry.visibility = wgpu::ShaderStage::Fragment;
    entry.texture.sampleType = (type == TextureType_Depth) ? wgpu::TextureSampleType::Depth : wgpu::TextureSampleType::Float;

    if(type == TextureType_Cube)
        entry.texture.viewDimension = wgpu::TextureViewDimension::Cube;
    else
        entry.texture.viewDimension = wgpu::TextureViewDimension::e2D;

    return entry;
}

wgpu::BindGroupLayoutEntry BuildSamplerLayoutEntry(uint32_t binding, bool isComparison)
{
    wgpu::BindGroupLayoutEntry entry{};
    entry.binding = binding;
    entry.visibility = wgpu::ShaderStage::Fragment;
    entry.sampler.type = isComparison ? wgpu::SamplerBindingType::Comparison : wgpu::SamplerBindingType::Filtering;
    return entry;
}

wgpu::BindGroupLayoutEntry BuildBufferLayoutEntry(uint32_t binding, BufferType type, bool isDynamic, size_t size)
{
    wgpu::BindGroupLayoutEntry entry{};
    entry.binding = binding;
    entry.visibility = type == BufferType::Uniform ? wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment : wgpu::ShaderStage::Fragment;
    entry.buffer.type = type == BufferType::Uniform
        ? wgpu::BufferBindingType::Uniform
        : wgpu::BufferBindingType::ReadOnlyStorage;
    entry.buffer.minBindingSize = size;
    entry.buffer.hasDynamicOffset = isDynamic;
    return entry;
}

#pragma endregion

Shader2::Shader2(const char* name) : m_Name(name), _impl(std::make_shared<Impl>())
{}

Shader2::~Shader2() = default;

void Shader2::BuildBindgroupLayouts()
{
    _impl->m_BindgroupLayouts.resize(m_BindGroups.size());
    for(size_t i{0}; i < m_BindGroups.size(); ++i)
    {
        BindGroup& group = m_BindGroups[i];
        std::vector<ShaderResource>& resources = group.GetResources();
        std::vector<wgpu::BindGroupLayoutEntry> layoutEntries;
        layoutEntries.reserve(resources.size());

        for (const ShaderResource& res : resources)
        {
            if (std::holds_alternative<BufferResource>(res))
            {
                layoutEntries.push_back(BuildBufferLayoutEntry(std::get<BufferResource>(res).binding, std::get<BufferResource>(res).buffer->GetType(), std::get<BufferResource>(res).buffer->IsDynamic(), std::get<BufferResource>(res).buffer->GetSize()));
            }
            else if (std::holds_alternative<TextureResource>(res))
            {
                layoutEntries.push_back(BuildTextureLayoutEntry(std::get<TextureResource>(res).binding, std::get<TextureResource>(res).textureType));
            }
            else if (std::holds_alternative<SamplerResource>(res))
            {
                layoutEntries.push_back(BuildSamplerLayoutEntry(std::get<SamplerResource>(res).binding, std::get<SamplerResource>(res).isComparison));
            }
        }
        wgpu::BindGroupLayoutDescriptor bindgroupLayoutDesc{};
        std::string name(m_Name);
        name += " Bindground Layout noel ";
        name += std::to_string(i);
        bindgroupLayoutDesc.label = name.c_str();
        bindgroupLayoutDesc.entryCount = layoutEntries.size();
        bindgroupLayoutDesc.entries = layoutEntries.data();
        _impl->m_BindgroupLayouts[i] = device.CreateBindGroupLayout(&bindgroupLayoutDesc);
    }
}

void Shader2::BuildBindgroupFromLayout(BindGroup& bg, int i)
{
    std::vector<wgpu::BindGroupEntry> bindgroupEntries;
    for (const ShaderResource& res : bg.GetResources())
    {
        if (std::holds_alternative<BufferResource>(res))
        {
            const BufferResource& br = std::get<BufferResource>(res);
            wgpu::BindGroupEntry entry;
            // wgpu::BufferDescriptor bufferDesc;
            // bufferDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
            // bufferDesc.mappedAtCreation = false;
            // if(br.layout.IsDynamic())
            // {
            //     bufferDesc.size = br.layout.GetUniformStride() * 256;
            // }
            // else
            //     bufferDesc.size = br.layout.total_size();
            // br._impl->m_Buffer = device.CreateBuffer(&bufferDesc);
            
            //entry.buffer = br._impl->m_Buffer;
            entry.buffer = *reinterpret_cast<const wgpu::Buffer*>(br.buffer->GetBuffer());
            entry.offset = 0;
            entry.size = br.buffer->GetBindingSize();
            entry.binding = br.binding;
            bindgroupEntries.push_back(entry);
        }
        else if (std::holds_alternative<TextureResource>(res))
        {
            const TextureResource& tr = std::get<TextureResource>(res);
            wgpu::BindGroupEntry entry;
            entry.binding = tr.binding;
            if (tr.texture->GetTextureView() == nullptr)
            {
                Logger::Error("Texture view is null for texture: " + std::string(tr.name));
            }
            entry.textureView = *reinterpret_cast<wgpu::TextureView*>(tr.texture->GetTextureView());
            bindgroupEntries.push_back(entry);
        }
        else if (std::holds_alternative<SamplerResource>(res))
        {
            const SamplerResource& sr = std::get<SamplerResource>(res);
            wgpu::BindGroupEntry entry;
            wgpu::SamplerDescriptor samplerDesc{};
            if (sr.isComparison)
            {
                samplerDesc.addressModeU = wgpu::AddressMode::ClampToEdge;
                samplerDesc.addressModeV = wgpu::AddressMode::ClampToEdge;
                samplerDesc.addressModeW = wgpu::AddressMode::ClampToEdge;

                samplerDesc.minFilter = wgpu::FilterMode::Linear;
            }
            else
            {
                samplerDesc.addressModeU = wgpu::AddressMode::Repeat;
                samplerDesc.addressModeV = wgpu::AddressMode::Repeat;
                samplerDesc.addressModeW = wgpu::AddressMode::Repeat;

                samplerDesc.minFilter = wgpu::FilterMode::Nearest;
            }
            
            samplerDesc.magFilter = wgpu::FilterMode::Linear;
            
            samplerDesc.mipmapFilter = wgpu::MipmapFilterMode::Linear;
            samplerDesc.lodMinClamp = 0.0f;
            samplerDesc.lodMaxClamp = 1000.0f;
            samplerDesc.compare = sr.isComparison ? wgpu::CompareFunction::LessEqual : wgpu::CompareFunction::Undefined;
            samplerDesc.maxAnisotropy = 1;
            sr._impl->m_Sampler = device.CreateSampler(&samplerDesc);
            entry.binding = sr.binding;
            entry.sampler = sr._impl->m_Sampler; 
            bindgroupEntries.push_back(entry);
        }
    }

    wgpu::BindGroupDescriptor bindGroupDesc{};
    bindGroupDesc.layout = _impl->m_BindgroupLayouts[i];
    bindGroupDesc.entryCount = bindgroupEntries.size();
    bindGroupDesc.entries = bindgroupEntries.data();
    bg._impl->m_Bindgroup = device.CreateBindGroup(&bindGroupDesc);
}

void Shader2::BuildBindgroups()
{
    for (size_t i{0}; i < m_BindGroups.size(); ++i)
    {
        if (i == m_MaterialIndex) continue;
        BuildBindgroupFromLayout(m_BindGroups[i], i);
    }
}


Shader2& Shader2::Build(bool shadow)
{
    m_BindGroupLayoutMap.clear();
    for (size_t i{0}; i < m_BindGroups.size(); ++i)
    {
        if (i == m_MaterialIndex) continue;

        uint32_t dynamicBuffers = 0;
        for (size_t j = 0; j < m_BindGroups[i].GetResources().size(); ++j)
        {
            ShaderResource& res = m_BindGroups[i].GetResources()[j];
            if (std::holds_alternative<BufferResource>(res))
            {
                BufferResource& br = std::get<BufferResource>(res);
                br.group = static_cast<uint32_t>(i);
                if (br.buffer->IsDynamic())
                {
                    br.dynamicBufferOffsetIndex = dynamicBuffers;
                    dynamicBuffers++;
                }
                m_BindGroupLayoutMap[br.name] = (ShaderResource*)&br;
            }
            else if (std::holds_alternative<TextureResource>(res))
            {
                const TextureResource& tr = std::get<TextureResource>(res);
                m_BindGroupLayoutMap[tr.name] = (ShaderResource*)&tr;
            }
            else if (std::holds_alternative<SamplerResource>(res))
            {
                const SamplerResource& sr = std::get<SamplerResource>(res);
                m_BindGroupLayoutMap[sr.name] = (ShaderResource*)&sr;
            }
        }
    }
    for (const PendingTextureBind& ptb : m_PendingTextures)
    {
        auto it = m_BindGroupLayoutMap.find(ptb.name);
        if (it == m_BindGroupLayoutMap.end())
            continue;

        ShaderResource* res = it->second;
        if (!std::holds_alternative<TextureResource>(*res))
            continue;

        auto& tr = std::get<TextureResource>(*res);
        tr.texture = ptb.texture;
    }
    BuildBindgroupLayouts();
    BuildBindgroups();
    
    

    wgpu::ShaderSourceWGSL wgsl{{.code = m_ShaderSource.c_str()}};
    wgpu::ShaderModuleDescriptor shaderModuleDescriptor{.nextInChain = &wgsl};

    wgpu::ShaderModule shaderModule =
    device.CreateShaderModule(&shaderModuleDescriptor);
    const std::vector<TextureFormat>& outputFormats = m_Renderpass->GetOutputFormats();
    std::vector<wgpu::ColorTargetState> colorTargetStates(outputFormats.size());

    for(size_t i = 0; i < outputFormats.size(); i++)
    {
        colorTargetStates[i] = {
            .format = static_cast<wgpu::TextureFormat>((uint32_t)(outputFormats[i])),
            .writeMask = wgpu::ColorWriteMask::All,
        };
    }

    wgpu::FragmentState fragmentState{
    .module = shaderModule, .entryPoint = wgpu::StringView("fragmentMain"), .targetCount = outputFormats.size(), .targets = colorTargetStates.data()};

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
                                          .cullMode = shadow ? wgpu::CullMode::None : wgpu::CullMode::Back
                                        },
                                     .depthStencil = &depthStencilState,
                                     .multisample = {
                                        .count = 1,
                                        .mask = ~0u,
                                        .alphaToCoverageEnabled = false
                                     },
                                     .fragment = outputFormats.size() == 0 ? nullptr : &fragmentState};
    
    _impl->m_Pipeline = device.CreateRenderPipeline(&descriptor);
    
    return *this;
}

void* Shader2::GetPipeline()
{
    return &_impl->m_Pipeline;
}
void* Shader2::GetBindGroup(uint32_t index)
{
    return m_BindGroups[index].GetBindGroup();
}