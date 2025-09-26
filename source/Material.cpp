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

void IMaterial::InitMaterial(Shader& shader, std::vector<Texture> textures)
{
    m_Textures = textures;

    LoadSampler(-1, -1 , WrapMode::REPEAT, WrapMode::REPEAT);
    std::vector<wgpu::BindGroupEntry> bindings(6);
    bindings[0].binding = 0;
    bindings[0].textureView = m_Textures[0].GetTextureView();

    bindings[1].binding = 1;
    bindings[1].textureView = m_Textures[1].GetTextureView();

    bindings[2].binding = 2;
    bindings[2].textureView = m_Textures[2].GetTextureView();

    bindings[3].binding = 3;
    bindings[3].textureView = m_Textures[3].GetTextureView();

    bindings[4].binding = 4;
    bindings[4].textureView = m_Textures[4].GetTextureView();

    bindings[5].binding = 5;
    bindings[5].sampler = samplers[0];

    wgpu::BindGroupDescriptor bindGroupDesc;
    bindGroupDesc.layout = shader.GetTextureBindGroupLayout();
    bindGroupDesc.entryCount = (uint32_t)bindings.size();
    bindGroupDesc.entries = bindings.data();
    bindGroup = device.CreateBindGroup(&bindGroupDesc);
}
