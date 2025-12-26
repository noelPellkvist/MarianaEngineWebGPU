#include <Material.hpp>
#include "Init.hpp"
#include <FileReader.hpp>
#include <webgpu/webgpu_cpp.h>

struct IMaterial::Impl
{
    std::vector<wgpu::Sampler> samplers;
    wgpu::BindGroup MaterialBindGroup;
};

IMaterial::IMaterial(uint32_t index) : bufferIndex(index), impl(std::make_unique<Impl>())
{

}

IMaterial::~IMaterial()
{

}

void IMaterial::LoadSampler()
{
    wgpu::SamplerDescriptor samplerDesc{};
    samplerDesc.addressModeU = wgpu::AddressMode::Repeat;
    samplerDesc.addressModeV = wgpu::AddressMode::Repeat;
    samplerDesc.addressModeW = wgpu::AddressMode::Repeat;
    samplerDesc.magFilter = wgpu::FilterMode::Linear;
    samplerDesc.minFilter = wgpu::FilterMode::Nearest;
    samplerDesc.mipmapFilter = wgpu::MipmapFilterMode::Linear;
    samplerDesc.lodMinClamp = 0.0f;
    samplerDesc.lodMaxClamp = 1000.0f;
    samplerDesc.compare = wgpu::CompareFunction::Undefined;
    samplerDesc.maxAnisotropy = 1;
    impl->samplers.push_back(device.CreateSampler(&samplerDesc));
}

void IMaterial::InitMaterial(IShader& shader, std::vector<Texture> textures)
{
    m_Shader = &shader;
    m_Textures = textures;

    LoadSampler();
    std::vector<wgpu::BindGroupEntry> bindings(shader.GetTextureCount() + 2);

    bindings[0] = *static_cast<wgpu::BindGroupEntry*>(shader.GetMaterialBufferEntry());

    for(size_t i = 1; i < shader.GetTextureCount() + 1; i++)
    {
        if (i - 1 >= m_Textures.size())
            break;
        bindings[i].binding = i;
        bindings[i].textureView = *static_cast<wgpu::TextureView*>(m_Textures[i - 1].GetTextureView());
    }

    bindings[shader.GetTextureCount() + 1].binding = shader.GetTextureCount() + 1;
    bindings[shader.GetTextureCount() + 1].sampler = impl->samplers[0];

    wgpu::BindGroupDescriptor bindGroupDesc;
    bindGroupDesc.layout = *static_cast<wgpu::BindGroupLayout*>(shader.GetBindGroupLayout(shader.GetBindingsCount() - 1));
    bindGroupDesc.entryCount = (uint32_t)bindings.size();
    bindGroupDesc.entries = bindings.data();
    impl->MaterialBindGroup = device.CreateBindGroup(&bindGroupDesc);
}

void* IMaterial::GetBindGroup(uint32_t index)
{
    if(index == m_Shader->GetBindingsCount() - 1)
        return &impl->MaterialBindGroup;
    else
    {
        return m_Shader->GetBindGroup(index);
    }
}


Material2::Material2()
{

}

Material2::~Material2()
{

}

Material2& Material2::InitFromShader(Shader2& shader)
{
    m_Bindgroup.resources = shader.m_BindGroups[shader.m_MaterialIndex].resources;
    m_Shader = &shader;
    for (ShaderResource& res : m_Bindgroup.resources)
    {
        if (std::holds_alternative<UniformBufferResource>(res))
        {
            const UniformBufferResource& ubr = std::get<UniformBufferResource>(res);
            m_ResourceMap[ubr.name] = &res;
        }
        else if (std::holds_alternative<TextureResource>(res))
        {
            const TextureResource& tr = std::get<TextureResource>(res);
            m_ResourceMap[tr.name] = &res;
        }
        else if (std::holds_alternative<SamplerResource>(res))
        {
            const SamplerResource& sr = std::get<SamplerResource>(res);
            m_ResourceMap[sr.name] = &res;
        }
    }
    return *this;
}

 Material2& Material2::SetTexture(std::string name, Texture& texture)
 {
    ShaderResource& m = *m_ResourceMap[name];

    if (std::holds_alternative<TextureResource>(m))
    {
        std::get<TextureResource>(m).texture = &texture;
    }
    return *this;
 }

void* Material2::GetBindGroup()
{
    return m_Bindgroup.GetBindGroup();
}

Material2& Material2::Build()
{
    m_Shader->BuildBindgroupFromLayout(m_Bindgroup, m_Shader->m_MaterialIndex);
    return *this;
}
