#include <Material.hpp>
#include <Init.hpp>
#include <FileReader.hpp>


Material::Material()
{

}

Material::~Material()
{

}

void Material::LoadTexture(std::string texturePath)
{
    int width, height;
    std::vector<uint8_t> pixels = FileReader::LoadPixelsFromImage(texturePath, width, height);

    wgpu::TextureDescriptor textureDesc{};
    textureDesc.dimension = wgpu::TextureDimension::e2D;
    textureDesc.size = {(unsigned int)width, (unsigned int)height, 1};
    textureDesc.mipLevelCount = 1;
    textureDesc.sampleCount = 1;
    textureDesc.format = wgpu::TextureFormat::RGBA8Unorm;
    textureDesc.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
    textureDesc.viewFormatCount = 0;
    textureDesc.viewFormats = nullptr;
    wgpu::Texture texture = device.CreateTexture(&textureDesc);

    wgpu::TextureViewDescriptor textureViewDesc{};
    textureViewDesc.aspect = wgpu::TextureAspect::All;
    textureViewDesc.baseArrayLayer = 0;
    textureViewDesc.arrayLayerCount = 1;
    textureViewDesc.baseMipLevel = 0;
    textureViewDesc.mipLevelCount = 1;
    textureViewDesc.dimension = wgpu::TextureViewDimension::e2D;
    textureViewDesc.format = textureDesc.format;
    wgpu::TextureView textureView = texture.CreateView(&textureViewDesc);

    wgpu::TexelCopyTextureInfo destination;
    destination.texture = texture;
    destination.mipLevel = 0;
    destination.origin = {0, 0, 0};
    destination.aspect = wgpu::TextureAspect::All;

    wgpu::TexelCopyBufferLayout source;
    source.offset = 0;
    source.bytesPerRow = 4 * textureDesc.size.width;
    source.rowsPerImage = textureDesc.size.height;

    device.GetQueue().WriteTexture(&destination, pixels.data(), pixels.size(), &source, &textureDesc.size);

    textures.push_back(texture);
    textureViews.push_back(textureView);
}

void Material::LoadSampler(int minFilter, int magFilter, WrapMode wrapS, WrapMode wrapT)
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

void Material::InitMaterial(Shader& shader, std::vector<std::string> textureNames)
{
    LoadTexture(textureNames[0]);
    LoadTexture(textureNames[1]);
    LoadTexture(textureNames[2]);
    LoadTexture(textureNames[3]);
    LoadSampler(-1, -1 , WrapMode::REPEAT, WrapMode::REPEAT);
    std::vector<wgpu::BindGroupEntry> bindings(5);
    bindings[0].binding = 0;
    bindings[0].textureView = textureViews[0];

    bindings[1].binding = 1;
    bindings[1].textureView = textureViews[1];

    bindings[2].binding = 2;
    bindings[2].textureView = textureViews[2];

    bindings[3].binding = 3;
    bindings[3].textureView = textureViews[3];

    bindings[4].binding = 4;
    bindings[4].sampler = samplers[0];

    wgpu::BindGroupDescriptor bindGroupDesc;
    bindGroupDesc.layout = shader.GetTextureBindGroupLayout();
    bindGroupDesc.entryCount = (uint32_t)bindings.size();
    bindGroupDesc.entries = bindings.data();
    bindGroup = device.CreateBindGroup(&bindGroupDesc);
}
