#include <Sampler.hpp>
#include <Init.hpp>
#include <moved_later/tiny_gltf.h>

inline void MapGltfFilters(
    int gltfMinFilter, int gltfMagFilter,
    wgpu::FilterMode& outMinFilter,
    wgpu::FilterMode& outMagFilter,
    wgpu::MipmapFilterMode& outMipFilter,
    float& outLodMaxClamp)
{
    // Default if unspecified (-1). glTF’s default is effectively LINEAR sampling.
    const bool minUnspec = (gltfMinFilter == -1);
    const bool magUnspec = (gltfMagFilter == -1);

    // MAG filter (only NEAREST/LINEAR exist)
    if (!magUnspec && gltfMagFilter == TINYGLTF_TEXTURE_FILTER_NEAREST) {
        outMagFilter = wgpu::FilterMode::Nearest;
    } else {
        outMagFilter = wgpu::FilterMode::Linear; // default or LINEAR
    }

    // MIN + MIPMAP filter come from gltfMinFilter
    // If unspecified, many viewers default to trilinear (nice quality).
    if (minUnspec) {
        outMinFilter   = wgpu::FilterMode::Linear;
        outMipFilter   = wgpu::MipmapFilterMode::Linear;
        outLodMaxClamp = 1000.0f; // allow mips if the texture has them
        return;
    }

    switch (gltfMinFilter) {
        case TINYGLTF_TEXTURE_FILTER_NEAREST:
            outMinFilter   = wgpu::FilterMode::Nearest;
            outMipFilter   = wgpu::MipmapFilterMode::Nearest; // “no mips” in GL terms
            outLodMaxClamp = 0.0f;  // force base level
            break;
        case TINYGLTF_TEXTURE_FILTER_LINEAR:
            outMinFilter   = wgpu::FilterMode::Linear;
            outMipFilter   = wgpu::MipmapFilterMode::Nearest; // “no mips”
            outLodMaxClamp = 0.0f;
            break;
        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
            outMinFilter   = wgpu::FilterMode::Nearest;
            outMipFilter   = wgpu::MipmapFilterMode::Nearest;
            outLodMaxClamp = 1000.0f;
            break;
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
            outMinFilter   = wgpu::FilterMode::Linear;
            outMipFilter   = wgpu::MipmapFilterMode::Nearest;
            outLodMaxClamp = 1000.0f;
            break;
        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
            outMinFilter   = wgpu::FilterMode::Nearest;
            outMipFilter   = wgpu::MipmapFilterMode::Linear;
            outLodMaxClamp = 1000.0f;
            break;
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
            outMinFilter   = wgpu::FilterMode::Linear;
            outMipFilter   = wgpu::MipmapFilterMode::Linear;
            outLodMaxClamp = 1000.0f;
            break;
        default:
            // Unknown? Be gracefully fancy.
            outMinFilter   = wgpu::FilterMode::Linear;
            outMipFilter   = wgpu::MipmapFilterMode::Linear;
            outLodMaxClamp = 1000.0f;
            break;
    }
}

inline wgpu::AddressMode MapWrap(WrapMode mode) {
    switch (mode) {
        case REPEAT:           return wgpu::AddressMode::Repeat;
        case CLAMP_TO_EDGE:    return wgpu::AddressMode::ClampToEdge;
        case MIRRORED_REPEAT:  return wgpu::AddressMode::MirrorRepeat;
        default:               return wgpu::AddressMode::Repeat;
    }
}

wgpu::Sampler Sampler::CreateSampler(int minFilter, int magFilter, WrapMode wrapS, WrapMode wrapT)
{
    wgpu::SamplerDescriptor samplerDesc{};
    samplerDesc.addressModeU = MapWrap(wrapS);
    samplerDesc.addressModeV = MapWrap(wrapT);
    samplerDesc.addressModeW = wgpu::AddressMode::ClampToEdge;

    wgpu::FilterMode minF, magF;
    wgpu::MipmapFilterMode mipF;
    float lodMax = 1000.0f;
    MapGltfFilters(minFilter, magFilter, minF, magF, mipF, lodMax);

    samplerDesc.minFilter = minF;
    samplerDesc.magFilter = magF;
    samplerDesc.mipmapFilter = mipF;
    
    samplerDesc.lodMinClamp = 0.0f;
    samplerDesc.lodMaxClamp = lodMax;
    samplerDesc.maxAnisotropy = 1;
    samplerDesc.compare = wgpu::CompareFunction::Undefined;

    return device.CreateSampler(&samplerDesc);
}