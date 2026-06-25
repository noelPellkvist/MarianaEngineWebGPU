#include <Texture.hpp>
#include <FileReader.hpp>
#include "Init.hpp"
#include <webgpu/webgpu_cpp.h>

[[nodiscard]] wgpu::TextureFormat ToNative(TextureFormat f) noexcept {
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

[[nodiscard]] TextureFormat FromNative(void* native) noexcept {
    wgpu::TextureFormat f = *reinterpret_cast<wgpu::TextureFormat*>(native);
    switch (f) {
        case wgpu::TextureFormat::Undefined:            return TextureFormat::Undefined;
        case wgpu::TextureFormat::R8Unorm:              return TextureFormat::R8Unorm;
        case wgpu::TextureFormat::R8Snorm:              return TextureFormat::R8Snorm;
        case wgpu::TextureFormat::R8Uint:               return TextureFormat::R8Uint;
        case wgpu::TextureFormat::R8Sint:               return TextureFormat::R8Sint;
        case wgpu::TextureFormat::R16Unorm:             return TextureFormat::R16Unorm;
        case wgpu::TextureFormat::R16Snorm:             return TextureFormat::R16Snorm;
        case wgpu::TextureFormat::R16Uint:              return TextureFormat::R16Uint;
        case wgpu::TextureFormat::R16Sint:              return TextureFormat::R16Sint;
        case wgpu::TextureFormat::R16Float:             return TextureFormat::R16Float;
        case wgpu::TextureFormat::RG8Unorm:             return TextureFormat::RG8Unorm;
        case wgpu::TextureFormat::RG8Snorm:             return TextureFormat::RG8Snorm;
        case wgpu::TextureFormat::RG8Uint:              return TextureFormat::RG8Uint;
        case wgpu::TextureFormat::RG8Sint:              return TextureFormat::RG8Sint;
        case wgpu::TextureFormat::R32Float:             return TextureFormat::R32Float;
        case wgpu::TextureFormat::R32Uint:              return TextureFormat::R32Uint;
        case wgpu::TextureFormat::R32Sint:              return TextureFormat::R32Sint;
        case wgpu::TextureFormat::RG16Unorm:            return TextureFormat::RG16Unorm;
        case wgpu::TextureFormat::RG16Snorm:            return TextureFormat::RG16Snorm;
        case wgpu::TextureFormat::RG16Uint:             return TextureFormat::RG16Uint;
        case wgpu::TextureFormat::RG16Sint:             return TextureFormat::RG16Sint;
        case wgpu::TextureFormat::RG16Float:            return TextureFormat::RG16Float;
        case wgpu::TextureFormat::RGBA8Unorm:           return TextureFormat::RGBA8Unorm;
        case wgpu::TextureFormat::RGBA8UnormSrgb:       return TextureFormat::RGBA8UnormSrgb;
        case wgpu::TextureFormat::RGBA8Snorm:           return TextureFormat::RGBA8Snorm;
        case wgpu::TextureFormat::RGBA8Uint:            return TextureFormat::RGBA8Uint;
        case wgpu::TextureFormat::RGBA8Sint:            return TextureFormat::RGBA8Sint;
        case wgpu::TextureFormat::BGRA8Unorm:           return TextureFormat::BGRA8Unorm;
        case wgpu::TextureFormat::BGRA8UnormSrgb:       return TextureFormat::BGRA8UnormSrgb;
        case wgpu::TextureFormat::RGB10A2Uint:          return TextureFormat::RGB10A2Uint;
        case wgpu::TextureFormat::RGB10A2Unorm:         return TextureFormat::RGB10A2Unorm;
        case wgpu::TextureFormat::RG11B10Ufloat:        return TextureFormat::RG11B10Ufloat;
        case wgpu::TextureFormat::RGB9E5Ufloat:         return TextureFormat::RGB9E5Ufloat;
        case wgpu::TextureFormat::RG32Float:            return TextureFormat::RG32Float;
        case wgpu::TextureFormat::RG32Uint:             return TextureFormat::RG32Uint;
        case wgpu::TextureFormat::RG32Sint:             return TextureFormat::RG32Sint;
        case wgpu::TextureFormat::RGBA16Unorm:          return TextureFormat::RGBA16Unorm;
        case wgpu::TextureFormat::RGBA16Snorm:          return TextureFormat::RGBA16Snorm;
        case wgpu::TextureFormat::RGBA16Uint:           return TextureFormat::RGBA16Uint;
        case wgpu::TextureFormat::RGBA16Sint:           return TextureFormat::RGBA16Sint;
        case wgpu::TextureFormat::RGBA16Float:          return TextureFormat::RGBA16Float;
        case wgpu::TextureFormat::RGBA32Float:          return TextureFormat::RGBA32Float;
        case wgpu::TextureFormat::RGBA32Uint:           return TextureFormat::RGBA32Uint;
        case wgpu::TextureFormat::RGBA32Sint:           return TextureFormat::RGBA32Sint;
        case wgpu::TextureFormat::Stencil8:             return TextureFormat::Stencil8;
        case wgpu::TextureFormat::Depth16Unorm:         return TextureFormat::Depth16Unorm;
        case wgpu::TextureFormat::Depth24Plus:          return TextureFormat::Depth24Plus;
        case wgpu::TextureFormat::Depth24PlusStencil8:  return TextureFormat::Depth24PlusStencil8;
        case wgpu::TextureFormat::Depth32Float:         return TextureFormat::Depth32Float;
        case wgpu::TextureFormat::Depth32FloatStencil8: return TextureFormat::Depth32FloatStencil8;
        case wgpu::TextureFormat::BC1RGBAUnorm:         return TextureFormat::BC1RGBAUnorm;
        case wgpu::TextureFormat::BC1RGBAUnormSrgb:     return TextureFormat::BC1RGBAUnormSrgb;
        case wgpu::TextureFormat::BC2RGBAUnorm:         return TextureFormat::BC2RGBAUnorm;
        case wgpu::TextureFormat::BC2RGBAUnormSrgb:     return TextureFormat::BC2RGBAUnormSrgb;
        case wgpu::TextureFormat::BC3RGBAUnorm:         return TextureFormat::BC3RGBAUnorm;
        case wgpu::TextureFormat::BC3RGBAUnormSrgb:     return TextureFormat::BC3RGBAUnormSrgb;
        case wgpu::TextureFormat::BC4RUnorm:            return TextureFormat::BC4RUnorm;
        case wgpu::TextureFormat::BC4RSnorm:            return TextureFormat::BC4RSnorm;
        case wgpu::TextureFormat::BC5RGUnorm:           return TextureFormat::BC5RGUnorm;
        case wgpu::TextureFormat::BC5RGSnorm:           return TextureFormat::BC5RGSnorm;
        case wgpu::TextureFormat::BC6HRGBUfloat:        return TextureFormat::BC6HRGBUfloat;
        case wgpu::TextureFormat::BC6HRGBFloat:         return TextureFormat::BC6HRGBFloat;
        case wgpu::TextureFormat::BC7RGBAUnorm:         return TextureFormat::BC7RGBAUnorm;
        case wgpu::TextureFormat::BC7RGBAUnormSrgb:     return TextureFormat::BC7RGBAUnormSrgb;
        case wgpu::TextureFormat::ETC2RGB8Unorm:        return TextureFormat::ETC2RGB8Unorm;
        case wgpu::TextureFormat::ETC2RGB8UnormSrgb:    return TextureFormat::ETC2RGB8UnormSrgb;
        case wgpu::TextureFormat::ETC2RGB8A1Unorm:      return TextureFormat::ETC2RGB8A1Unorm;
        case wgpu::TextureFormat::ETC2RGB8A1UnormSrgb:  return TextureFormat::ETC2RGB8A1UnormSrgb;
        case wgpu::TextureFormat::ETC2RGBA8Unorm:       return TextureFormat::ETC2RGBA8Unorm;
        case wgpu::TextureFormat::ETC2RGBA8UnormSrgb:   return TextureFormat::ETC2RGBA8UnormSrgb;
        case wgpu::TextureFormat::EACR11Unorm:          return TextureFormat::EACR11Unorm;
        case wgpu::TextureFormat::EACR11Snorm:          return TextureFormat::EACR11Snorm;
        case wgpu::TextureFormat::EACRG11Unorm:         return TextureFormat::EACRG11Unorm;
        case wgpu::TextureFormat::EACRG11Snorm:         return TextureFormat::EACRG11Snorm;
        case wgpu::TextureFormat::ASTC4x4Unorm:         return TextureFormat::ASTC4x4Unorm;
        case wgpu::TextureFormat::ASTC4x4UnormSrgb:     return TextureFormat::ASTC4x4UnormSrgb;
        case wgpu::TextureFormat::ASTC5x4Unorm:         return TextureFormat::ASTC5x4Unorm;
        case wgpu::TextureFormat::ASTC5x4UnormSrgb:     return TextureFormat::ASTC5x4UnormSrgb;
        case wgpu::TextureFormat::ASTC5x5Unorm:         return TextureFormat::ASTC5x5Unorm;
        case wgpu::TextureFormat::ASTC5x5UnormSrgb:     return TextureFormat::ASTC5x5UnormSrgb;
        case wgpu::TextureFormat::ASTC6x5Unorm:         return TextureFormat::ASTC6x5Unorm;
        case wgpu::TextureFormat::ASTC6x5UnormSrgb:     return TextureFormat::ASTC6x5UnormSrgb;
        case wgpu::TextureFormat::ASTC6x6Unorm:         return TextureFormat::ASTC6x6Unorm;
        case wgpu::TextureFormat::ASTC6x6UnormSrgb:     return TextureFormat::ASTC6x6UnormSrgb;
        case wgpu::TextureFormat::ASTC8x5Unorm:         return TextureFormat::ASTC8x5Unorm;
        case wgpu::TextureFormat::ASTC8x5UnormSrgb:     return TextureFormat::ASTC8x5UnormSrgb;
        case wgpu::TextureFormat::ASTC8x6Unorm:         return TextureFormat::ASTC8x6Unorm;
        case wgpu::TextureFormat::ASTC8x6UnormSrgb:     return TextureFormat::ASTC8x6UnormSrgb;
        case wgpu::TextureFormat::ASTC8x8Unorm:         return TextureFormat::ASTC8x8Unorm;
        case wgpu::TextureFormat::ASTC8x8UnormSrgb:     return TextureFormat::ASTC8x8UnormSrgb;
        case wgpu::TextureFormat::ASTC10x5Unorm:        return TextureFormat::ASTC10x5Unorm;
        case wgpu::TextureFormat::ASTC10x5UnormSrgb:    return TextureFormat::ASTC10x5UnormSrgb;
        case wgpu::TextureFormat::ASTC10x6Unorm:        return TextureFormat::ASTC10x6Unorm;
        case wgpu::TextureFormat::ASTC10x6UnormSrgb:    return TextureFormat::ASTC10x6UnormSrgb;
        case wgpu::TextureFormat::ASTC10x8Unorm:        return TextureFormat::ASTC10x8Unorm;
        case wgpu::TextureFormat::ASTC10x8UnormSrgb:    return TextureFormat::ASTC10x8UnormSrgb;
        case wgpu::TextureFormat::ASTC10x10Unorm:       return TextureFormat::ASTC10x10Unorm;
        case wgpu::TextureFormat::ASTC10x10UnormSrgb:   return TextureFormat::ASTC10x10UnormSrgb;
        case wgpu::TextureFormat::ASTC12x10Unorm:       return TextureFormat::ASTC12x10Unorm;
        case wgpu::TextureFormat::ASTC12x10UnormSrgb:   return TextureFormat::ASTC12x10UnormSrgb;
        case wgpu::TextureFormat::ASTC12x12Unorm:       return TextureFormat::ASTC12x12Unorm;
        case wgpu::TextureFormat::ASTC12x12UnormSrgb:   return TextureFormat::ASTC12x12UnormSrgb;
        default:                                        return TextureFormat::Undefined;
    }
}

struct ReadbackSlot {
    wgpu::Buffer buffer;
    bool         mapped = false;
};

struct Texture::Impl
{
    wgpu::Texture m_Texture;
    wgpu::TextureView m_View;
    ReadbackSlot slots[2];
    std::atomic<uint64_t> lastValue{~uint64_t{0}}; 
    bool         inited = false;
    int          dstIndex = 0;
};

static constexpr uint64_t kRowPitch   = 256;
static constexpr uint64_t kCopySize   = kRowPitch; // one row
static constexpr uint64_t kOffset     = 0;
static constexpr uint64_t kBytesToRead = 8;

static wgpu::Buffer MakeReadbackBuffer() {
    wgpu::BufferDescriptor desc{};
    desc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
    desc.size  = kCopySize; 
    return device.CreateBuffer(&desc);
}

Texture::Texture() : _impl(std::make_shared<Impl>())
{}

Texture::~Texture() = default;

void* Texture::GetTexture()
{
    return &_impl->m_Texture;
}

void* Texture::GetTextureView()
{
    return &_impl->m_View;
}



void Texture::LoadTexture(const std::string& path, TextureFormat format)
{
    int width, height;
    std::vector<uint8_t> pixels = FileReader::LoadPixelsFromImage(path, width, height);
    CreateTexture(width, height, format);
    UploadTexture(pixels.data(), pixels.size(), width, height);
    m_Format = format;
}

void Texture::CreateRenderTexture(TextureFormat format, int width, int height, bool MSSA)
{
    CreateTexture(width, height, format, MSSA, true, false);
    m_Format = format;
}

void Texture::CreateDepthTexture(TextureFormat format, int width, int height, bool MSSA)
{
    CreateTexture(width, height, format, MSSA, true, true);
    m_Format = format;
}

void Texture::UploadTexture(const uint8_t* pixels, size_t length, int width, int height)
{
    wgpu::TexelCopyTextureInfo destination;
    destination.texture = _impl->m_Texture;
    destination.mipLevel = 0;
    destination.origin = {0, 0, 0};
    destination.aspect = wgpu::TextureAspect::All;

    wgpu::TexelCopyBufferLayout source;
    source.offset = 0;
    source.bytesPerRow = 4 * (unsigned int)width;
    source.rowsPerImage = (unsigned int)height;

    wgpu::Extent3D extent = {(unsigned int)width, (unsigned int)height, 1};
    device.GetQueue().WriteTexture(&destination, pixels, length, &source, &extent);
}

void Texture::LoadCubeTexture(const std::vector<std::string> paths, TextureFormat format)
{
    int k_width, k_height;
    
    assert(paths.size() == 6 && "Cube texture requires 6 image paths.");
    

    int width, height;
    std::vector<std::vector<uint8_t>> facePixels(6);
    facePixels[0] = FileReader::LoadPixelsFromImage(paths[0], k_width, k_height);
    for (size_t i = 1; i < paths.size(); ++i) {
        facePixels[i] = FileReader::LoadPixelsFromImage(paths[i], width, height);
        assert(width == k_width && "Texture dimensions must match.");
        assert(height == k_height && "Texture dimensions must match.");
    }

    wgpu::TextureFormat textureFormat = ToNative(format);
    wgpu::TextureDescriptor textureDesc;
    textureDesc.dimension = wgpu::TextureDimension::e2D;
    textureDesc.format = textureFormat;
    textureDesc.mipLevelCount = 1;
    textureDesc.sampleCount = 1;
    textureDesc.size = {static_cast<unsigned int>(width), static_cast<unsigned int>(height), 6};
    textureDesc.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
    textureDesc.viewFormatCount = 1;
    textureDesc.viewFormats = &textureFormat;
    _impl->m_Texture = device.CreateTexture(&textureDesc);

    for (size_t i = 0; i < facePixels.size(); ++i) {
        wgpu::TexelCopyTextureInfo destination;
        destination.texture = _impl->m_Texture;
        destination.mipLevel = 0;
        destination.origin = {0, 0, static_cast<uint32_t>(i)};
        destination.aspect = wgpu::TextureAspect::All;

        wgpu::TexelCopyBufferLayout source;
        source.offset = 0;
        source.bytesPerRow = 4 * width;
        source.rowsPerImage = height;

        wgpu::Extent3D size = {static_cast<unsigned int>(width), static_cast<unsigned int>(height), 1};
        device.GetQueue().WriteTexture(&destination, facePixels[i].data(), facePixels[i].size(), &source, &size);
    }
    

    wgpu::TextureViewDescriptor textureViewDesc;
    textureViewDesc.aspect = wgpu::TextureAspect::All;
    textureViewDesc.baseArrayLayer = 0;
    textureViewDesc.arrayLayerCount = 6;
    textureViewDesc.baseMipLevel = 0;
    textureViewDesc.mipLevelCount = 1;
    textureViewDesc.dimension = wgpu::TextureViewDimension::Cube;
    textureViewDesc.format = textureFormat;
    _impl->m_View = _impl->m_Texture.CreateView(&textureViewDesc);
}

void Texture::CreateTexture(int width, int height, TextureFormat format, bool MSSA, bool renderTarget, bool isDepthTexture)
{
    m_Width = width;
    m_Height = height;

    wgpu::TextureDescriptor textureDesc{};
    textureDesc.dimension = wgpu::TextureDimension::e2D;
    textureDesc.size = {(unsigned int)width, (unsigned int)height, 1};
    textureDesc.mipLevelCount = 1;
    textureDesc.sampleCount = MSSA ? 4 : 1;
    textureDesc.format = ToNative(format);
    textureDesc.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopySrc | (renderTarget ? wgpu::TextureUsage::RenderAttachment : wgpu::TextureUsage::CopyDst);
    textureDesc.viewFormatCount = 0;
    textureDesc.viewFormats = nullptr;
    _impl->m_Texture = device.CreateTexture(&textureDesc);

    wgpu::TextureViewDescriptor textureViewDesc{};
    textureViewDesc.aspect = (isDepthTexture ? wgpu::TextureAspect::DepthOnly : wgpu::TextureAspect::All);
    textureViewDesc.baseArrayLayer = 0;
    textureViewDesc.arrayLayerCount = 1;
    textureViewDesc.baseMipLevel = 0;
    textureViewDesc.mipLevelCount = 1;
    textureViewDesc.dimension = wgpu::TextureViewDimension::e2D;
    textureViewDesc.format = textureDesc.format;
    _impl->m_View = _impl->m_Texture.CreateView(&textureViewDesc);
    m_Format = format;
}

uint64_t Texture::SamplePixel(int x, int y)
{
    auto& impl = *_impl;

    if (!impl.inited) {
        impl.slots[0].buffer = MakeReadbackBuffer();
        impl.slots[1].buffer = MakeReadbackBuffer();
        impl.inited = true;
        impl.lastValue.store(~uint64_t{0}, std::memory_order_relaxed);
    }

    ReadbackSlot& dst = impl.slots[impl.dstIndex];
    ReadbackSlot& src = impl.slots[impl.dstIndex ^ 1]; // the other one (likely mapped)

    // Ensure the destination buffer is unmapped before using as CopyDst.
    if (dst.mapped) {
        dst.buffer.Unmap();
        dst.mapped = false;
    }

    // Encode a 1x1 copy from (x,y) into dst.
    wgpu::TexelCopyTextureInfo srcTex{};
    srcTex.texture  = impl.m_Texture;
    srcTex.mipLevel = 0;
    srcTex.origin   = { static_cast<uint32_t>(x), static_cast<uint32_t>(y), 0 };
    srcTex.aspect   = wgpu::TextureAspect::All;

    wgpu::TexelCopyBufferInfo dstBuf{};
    dstBuf.buffer = dst.buffer;
    dstBuf.layout.offset        = kOffset;
    dstBuf.layout.bytesPerRow   = static_cast<uint32_t>(kRowPitch);
    dstBuf.layout.rowsPerImage  = 1;

    wgpu::Extent3D extent{1, 1, 1};

    wgpu::CommandEncoder enc = device.CreateCommandEncoder();
    enc.CopyTextureToBuffer(&srcTex, &dstBuf, &extent);
    wgpu::CommandBuffer cb = enc.Finish();
    device.GetQueue().Submit(1, &cb);

    ReadbackSlot* dstSlot = &dst;
    Impl* implPtr = &impl;
    (void)dstSlot->buffer.MapAsync(
        wgpu::MapMode::Read,
        kOffset,
        kCopySize,
        wgpu::CallbackMode::AllowSpontaneous,    // <-- new required arg
        [implPtr, dstSlot](wgpu::MapAsyncStatus status, wgpu::StringView /*message*/) {
            if (status == wgpu::MapAsyncStatus::Success) {
                const void* p = dstSlot->buffer.GetConstMappedRange(kOffset, kBytesToRead);
                if (p) {
                    uint16_t lanes[4]{};
                    std::memcpy(lanes, p, sizeof(lanes));
                    uint64_t v = static_cast<uint64_t>(lanes[0]) |
                                 (static_cast<uint64_t>(lanes[1]) << 16) |
                                 (static_cast<uint64_t>(lanes[2]) << 32) |
                                 (static_cast<uint64_t>(lanes[3]) << 48);
                    implPtr->lastValue.store(v, std::memory_order_relaxed);
                    dstSlot->mapped = true; // keep it mapped until reused as CopyDst
                } else {
                    dstSlot->mapped = false;
                }
            } else {
                // mapping failed; keep prior lastValue
                dstSlot->mapped = false;
            }
        }
    );

    uint64_t outValue = impl.lastValue.load(std::memory_order_relaxed);

    // Next call: flip roles.
    impl.dstIndex ^= 1;

    return outValue;
}