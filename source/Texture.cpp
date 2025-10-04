#include <Texture.hpp>
#include <FileReader.hpp>
#include <Init.hpp>

[[nodiscard]] constexpr wgpu::TextureFormat ToWGPU(TextureFormat f) noexcept {
    switch (f) {
        case TextureFormat::Undefined:             return wgpu::TextureFormat::Undefined;
        case TextureFormat::R8Unorm:               return wgpu::TextureFormat::R8Unorm;
        case TextureFormat::R8Snorm:               return wgpu::TextureFormat::R8Snorm;
        case TextureFormat::R8Uint:                return wgpu::TextureFormat::R8Uint;
        case TextureFormat::R8Sint:                return wgpu::TextureFormat::R8Sint;
        case TextureFormat::R16Unorm:              return wgpu::TextureFormat::R16Unorm;
        case TextureFormat::R16Snorm:              return wgpu::TextureFormat::R16Snorm;
        case TextureFormat::R16Uint:               return wgpu::TextureFormat::R16Uint;
        case TextureFormat::R16Sint:               return wgpu::TextureFormat::R16Sint;
        case TextureFormat::R16Float:              return wgpu::TextureFormat::R16Float;
        case TextureFormat::RG8Unorm:              return wgpu::TextureFormat::RG8Unorm;
        case TextureFormat::RG8Snorm:              return wgpu::TextureFormat::RG8Snorm;
        case TextureFormat::RG8Uint:               return wgpu::TextureFormat::RG8Uint;
        case TextureFormat::RG8Sint:               return wgpu::TextureFormat::RG8Sint;
        case TextureFormat::R32Float:              return wgpu::TextureFormat::R32Float;
        case TextureFormat::R32Uint:               return wgpu::TextureFormat::R32Uint;
        case TextureFormat::R32Sint:               return wgpu::TextureFormat::R32Sint;
        case TextureFormat::RG16Unorm:             return wgpu::TextureFormat::RG16Unorm;
        case TextureFormat::RG16Snorm:             return wgpu::TextureFormat::RG16Snorm;
        case TextureFormat::RG16Uint:              return wgpu::TextureFormat::RG16Uint;
        case TextureFormat::RG16Sint:              return wgpu::TextureFormat::RG16Sint;
        case TextureFormat::RG16Float:             return wgpu::TextureFormat::RG16Float;
        case TextureFormat::RGBA8Unorm:            return wgpu::TextureFormat::RGBA8Unorm;
        case TextureFormat::RGBA8UnormSrgb:        return wgpu::TextureFormat::RGBA8UnormSrgb;
        case TextureFormat::RGBA8Snorm:            return wgpu::TextureFormat::RGBA8Snorm;
        case TextureFormat::RGBA8Uint:             return wgpu::TextureFormat::RGBA8Uint;
        case TextureFormat::RGBA8Sint:             return wgpu::TextureFormat::RGBA8Sint;
        case TextureFormat::BGRA8Unorm:            return wgpu::TextureFormat::BGRA8Unorm;
        case TextureFormat::BGRA8UnormSrgb:        return wgpu::TextureFormat::BGRA8UnormSrgb;
        case TextureFormat::RGB10A2Uint:           return wgpu::TextureFormat::RGB10A2Uint;
        case TextureFormat::RGB10A2Unorm:          return wgpu::TextureFormat::RGB10A2Unorm;
        case TextureFormat::RG11B10Ufloat:         return wgpu::TextureFormat::RG11B10Ufloat;
        case TextureFormat::RGB9E5Ufloat:          return wgpu::TextureFormat::RGB9E5Ufloat;
        case TextureFormat::RG32Float:             return wgpu::TextureFormat::RG32Float;
        case TextureFormat::RG32Uint:              return wgpu::TextureFormat::RG32Uint;
        case TextureFormat::RG32Sint:              return wgpu::TextureFormat::RG32Sint;
        case TextureFormat::RGBA16Unorm:           return wgpu::TextureFormat::RGBA16Unorm;
        case TextureFormat::RGBA16Snorm:           return wgpu::TextureFormat::RGBA16Snorm;
        case TextureFormat::RGBA16Uint:            return wgpu::TextureFormat::RGBA16Uint;
        case TextureFormat::RGBA16Sint:            return wgpu::TextureFormat::RGBA16Sint;
        case TextureFormat::RGBA16Float:           return wgpu::TextureFormat::RGBA16Float;
        case TextureFormat::RGBA32Float:           return wgpu::TextureFormat::RGBA32Float;
        case TextureFormat::RGBA32Uint:            return wgpu::TextureFormat::RGBA32Uint;
        case TextureFormat::RGBA32Sint:            return wgpu::TextureFormat::RGBA32Sint;
        case TextureFormat::Stencil8:              return wgpu::TextureFormat::Stencil8;
        case TextureFormat::Depth16Unorm:          return wgpu::TextureFormat::Depth16Unorm;
        case TextureFormat::Depth24Plus:           return wgpu::TextureFormat::Depth24Plus;
        case TextureFormat::Depth24PlusStencil8:   return wgpu::TextureFormat::Depth24PlusStencil8;
        case TextureFormat::Depth32Float:          return wgpu::TextureFormat::Depth32Float;
        case TextureFormat::Depth32FloatStencil8:  return wgpu::TextureFormat::Depth32FloatStencil8;
        case TextureFormat::BC1RGBAUnorm:          return wgpu::TextureFormat::BC1RGBAUnorm;
        case TextureFormat::BC1RGBAUnormSrgb:      return wgpu::TextureFormat::BC1RGBAUnormSrgb;
        case TextureFormat::BC2RGBAUnorm:          return wgpu::TextureFormat::BC2RGBAUnorm;
        case TextureFormat::BC2RGBAUnormSrgb:      return wgpu::TextureFormat::BC2RGBAUnormSrgb;
        case TextureFormat::BC3RGBAUnorm:          return wgpu::TextureFormat::BC3RGBAUnorm;
        case TextureFormat::BC3RGBAUnormSrgb:      return wgpu::TextureFormat::BC3RGBAUnormSrgb;
        case TextureFormat::BC4RUnorm:             return wgpu::TextureFormat::BC4RUnorm;
        case TextureFormat::BC4RSnorm:             return wgpu::TextureFormat::BC4RSnorm;
        case TextureFormat::BC5RGUnorm:            return wgpu::TextureFormat::BC5RGUnorm;
        case TextureFormat::BC5RGSnorm:            return wgpu::TextureFormat::BC5RGSnorm;
        case TextureFormat::BC6HRGBUfloat:         return wgpu::TextureFormat::BC6HRGBUfloat;
        case TextureFormat::BC6HRGBFloat:          return wgpu::TextureFormat::BC6HRGBFloat;
        case TextureFormat::BC7RGBAUnorm:          return wgpu::TextureFormat::BC7RGBAUnorm;
        case TextureFormat::BC7RGBAUnormSrgb:      return wgpu::TextureFormat::BC7RGBAUnormSrgb;
        case TextureFormat::ETC2RGB8Unorm:         return wgpu::TextureFormat::ETC2RGB8Unorm;
        case TextureFormat::ETC2RGB8UnormSrgb:     return wgpu::TextureFormat::ETC2RGB8UnormSrgb;
        case TextureFormat::ETC2RGB8A1Unorm:       return wgpu::TextureFormat::ETC2RGB8A1Unorm;
        case TextureFormat::ETC2RGB8A1UnormSrgb:   return wgpu::TextureFormat::ETC2RGB8A1UnormSrgb;
        case TextureFormat::ETC2RGBA8Unorm:        return wgpu::TextureFormat::ETC2RGBA8Unorm;
        case TextureFormat::ETC2RGBA8UnormSrgb:    return wgpu::TextureFormat::ETC2RGBA8UnormSrgb;
        case TextureFormat::EACR11Unorm:           return wgpu::TextureFormat::EACR11Unorm;
        case TextureFormat::EACR11Snorm:           return wgpu::TextureFormat::EACR11Snorm;
        case TextureFormat::EACRG11Unorm:          return wgpu::TextureFormat::EACRG11Unorm;
        case TextureFormat::EACRG11Snorm:          return wgpu::TextureFormat::EACRG11Snorm;
        case TextureFormat::ASTC4x4Unorm:          return wgpu::TextureFormat::ASTC4x4Unorm;
        case TextureFormat::ASTC4x4UnormSrgb:      return wgpu::TextureFormat::ASTC4x4UnormSrgb;
        case TextureFormat::ASTC5x4Unorm:          return wgpu::TextureFormat::ASTC5x4Unorm;
        case TextureFormat::ASTC5x4UnormSrgb:      return wgpu::TextureFormat::ASTC5x4UnormSrgb;
        case TextureFormat::ASTC5x5Unorm:          return wgpu::TextureFormat::ASTC5x5Unorm;
        case TextureFormat::ASTC5x5UnormSrgb:      return wgpu::TextureFormat::ASTC5x5UnormSrgb;
        case TextureFormat::ASTC6x5Unorm:          return wgpu::TextureFormat::ASTC6x5Unorm;
        case TextureFormat::ASTC6x5UnormSrgb:      return wgpu::TextureFormat::ASTC6x5UnormSrgb;
        case TextureFormat::ASTC6x6Unorm:          return wgpu::TextureFormat::ASTC6x6Unorm;
        case TextureFormat::ASTC6x6UnormSrgb:      return wgpu::TextureFormat::ASTC6x6UnormSrgb;
        case TextureFormat::ASTC8x5Unorm:          return wgpu::TextureFormat::ASTC8x5Unorm;
        case TextureFormat::ASTC8x5UnormSrgb:      return wgpu::TextureFormat::ASTC8x5UnormSrgb;
        case TextureFormat::ASTC8x6Unorm:          return wgpu::TextureFormat::ASTC8x6Unorm;
        case TextureFormat::ASTC8x6UnormSrgb:      return wgpu::TextureFormat::ASTC8x6UnormSrgb;
        case TextureFormat::ASTC8x8Unorm:          return wgpu::TextureFormat::ASTC8x8Unorm;
        case TextureFormat::ASTC8x8UnormSrgb:      return wgpu::TextureFormat::ASTC8x8UnormSrgb;
        case TextureFormat::ASTC10x5Unorm:         return wgpu::TextureFormat::ASTC10x5Unorm;
        case TextureFormat::ASTC10x5UnormSrgb:     return wgpu::TextureFormat::ASTC10x5UnormSrgb;
        case TextureFormat::ASTC10x6Unorm:         return wgpu::TextureFormat::ASTC10x6Unorm;
        case TextureFormat::ASTC10x6UnormSrgb:     return wgpu::TextureFormat::ASTC10x6UnormSrgb;
        case TextureFormat::ASTC10x8Unorm:         return wgpu::TextureFormat::ASTC10x8Unorm;
        case TextureFormat::ASTC10x8UnormSrgb:     return wgpu::TextureFormat::ASTC10x8UnormSrgb;
        case TextureFormat::ASTC10x10Unorm:        return wgpu::TextureFormat::ASTC10x10Unorm;
        case TextureFormat::ASTC10x10UnormSrgb:    return wgpu::TextureFormat::ASTC10x10UnormSrgb;
        case TextureFormat::ASTC12x10Unorm:        return wgpu::TextureFormat::ASTC12x10Unorm;
        case TextureFormat::ASTC12x10UnormSrgb:    return wgpu::TextureFormat::ASTC12x10UnormSrgb;
        case TextureFormat::ASTC12x12Unorm:        return wgpu::TextureFormat::ASTC12x12Unorm;
        case TextureFormat::ASTC12x12UnormSrgb:    return wgpu::TextureFormat::ASTC12x12UnormSrgb;
    }
    return wgpu::TextureFormat::Undefined;
}

Texture::Texture()
{}

Texture::~Texture()
{}

void Texture::LoadTexture(const std::string& path, TextureFormat format)
{
    int width, height;
    std::vector<uint8_t> pixels = FileReader::LoadPixelsFromImage(path, width, height);
    LoadTexture(pixels.data(), pixels.size(), width, height, format);
}

void Texture::LoadTexture(const uint8_t* pixels, size_t length, int width, int height, TextureFormat format, bool MSSA, bool renderTarget, bool isDepthTexture)
{
    m_Width = width;
    m_Height = height;

    wgpu::TextureDescriptor textureDesc{};
    textureDesc.dimension = wgpu::TextureDimension::e2D;
    textureDesc.size = {(unsigned int)width, (unsigned int)height, 1};
    textureDesc.mipLevelCount = 1;
    textureDesc.sampleCount = MSSA ? 4 : 1;
    textureDesc.format = ToWGPU(format);
    textureDesc.usage = wgpu::TextureUsage::TextureBinding | (renderTarget ? wgpu::TextureUsage::RenderAttachment : wgpu::TextureUsage::CopyDst);
    textureDesc.viewFormatCount = 0;
    textureDesc.viewFormats = nullptr;
    m_Texture = device.CreateTexture(&textureDesc);

    wgpu::TextureViewDescriptor textureViewDesc{};
    textureViewDesc.aspect = (isDepthTexture ? wgpu::TextureAspect::DepthOnly : wgpu::TextureAspect::All);
    textureViewDesc.baseArrayLayer = 0;
    textureViewDesc.arrayLayerCount = 1;
    textureViewDesc.baseMipLevel = 0;
    textureViewDesc.mipLevelCount = 1;
    textureViewDesc.dimension = wgpu::TextureViewDimension::e2D;
    textureViewDesc.format = textureDesc.format;
    m_View = m_Texture.CreateView(&textureViewDesc);

    wgpu::TexelCopyTextureInfo destination;
    destination.texture = m_Texture;
    destination.mipLevel = 0;
    destination.origin = {0, 0, 0};
    destination.aspect = wgpu::TextureAspect::All;

    wgpu::TexelCopyBufferLayout source;
    source.offset = 0;
    source.bytesPerRow = 4 * textureDesc.size.width;
    source.rowsPerImage = textureDesc.size.height;

    device.GetQueue().WriteTexture(&destination, pixels, length, &source, &textureDesc.size);
}