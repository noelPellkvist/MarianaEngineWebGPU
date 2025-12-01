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