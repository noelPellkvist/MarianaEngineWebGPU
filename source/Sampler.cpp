#include <Sampler.hpp>

wgpu::AddressMode GetAddressMode(WrapMode mode)
{
    switch (mode)
    {
        case REPEAT:
            return wgpu::AddressMode::Repeat;
        case CLAMP_TO_EDGE:
            return wgpu::AddressMode::ClampToEdge;
        case MIRRORED_REPEAT:
            return wgpu::AddressMode::MirrorRepeat;
        default:
            return wgpu::AddressMode::Repeat;
    }
}

wgpu::Sampler Sampler::CreateSampler()
{
    wgpu::SamplerDescriptor samplerDesc{};
    samplerDesc.addressModeU = static_cast<wgpu::AddressMode>(wrapS);
    samplerDesc.addressModeV = static_cast<wgpu::AddressMode>(wrapT);
    samplerDesc.addressModeW = wgpu::AddressMode::ClampToEdge;
    samplerDesc.magFilter = static_cast<wgpu::FilterMode>(magFilter);
    samplerDesc.minFilter = static_cast<wgpu::FilterMode>(minFilter);
    samplerDesc.mipmapFilter = wgpu::FilterMode::Linear;
    samplerDesc.lodMinClamp = 0.0f;
    samplerDesc.lodMaxClamp = 1000.0f;
    samplerDesc.maxAnisotropy = 1;
    samplerDesc.compare = wgpu::CompareFunction::Undefined;

    return device.CreateSampler(&samplerDesc);
}