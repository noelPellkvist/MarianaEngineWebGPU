#include <Material.hpp>
#include <Init.hpp>
#include <FileReader.hpp>


IMaterial::IMaterial()
{

}

IMaterial::~IMaterial()
{

}

void IMaterial::LoadSampler(int minFilter, int magFilter, WrapMode wrapS, WrapMode wrapT)
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
    samplers.push_back(device.CreateSampler(&samplerDesc));
}

void IMaterial::InitMaterial(IShader& shader, std::vector<Texture> textures)
{
    m_Shader = &shader;
    m_Textures = textures;

    LoadSampler(-1, -1 , WrapMode::REPEAT, WrapMode::REPEAT);
    std::vector<wgpu::BindGroupEntry> bindings(shader.GetTextureCount() + 2);

    bindings[0] = shader.GetMaterialBufferEntry();

    for(size_t i = 1; i < shader.GetTextureCount() + 1; i++)
    {
        bindings[i].binding = i;
        bindings[i].textureView = m_Textures[i - 1].GetTextureView();
    }

    bindings[shader.GetTextureCount() + 1].binding = shader.GetTextureCount() + 1;
    bindings[shader.GetTextureCount() + 1].sampler = samplers[0];

    wgpu::BindGroupDescriptor bindGroupDesc;
    bindGroupDesc.layout = shader.GetBindGroupLayout(2);
    bindGroupDesc.entryCount = (uint32_t)bindings.size();
    bindGroupDesc.entries = bindings.data();
    MaterialBindGroup = device.CreateBindGroup(&bindGroupDesc);
}

const wgpu::BindGroup& IMaterial::GetBindGroup(uint32_t index)
{
    if(index == 2)
        return MaterialBindGroup;
    else
    {
        return m_Shader->GetBindGroup(index);
    }
}