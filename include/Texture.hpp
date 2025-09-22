#pragma once
#include <string>
#include <vector>
#include <webgpu/webgpu_cpp.h>

enum class TextureFormat : uint32_t {
    Undefined          = 0x0000,
    R8Unorm            = 0x0001,
    R8Snorm            = 0x0002,
    R8Uint             = 0x0003,
    R8Sint             = 0x0004,
    R16Unorm           = 0x0005,
    R16Snorm           = 0x0006,
    R16Uint            = 0x0007,
    R16Sint            = 0x0008,
    R16Float           = 0x0009,
    RG8Unorm           = 0x000A,
    RG8Snorm           = 0x000B,
    RG8Uint            = 0x000C,
    RG8Sint            = 0x000D,
    R32Float           = 0x000E,
    R32Uint            = 0x000F,
    R32Sint            = 0x0010,
    RG16Unorm          = 0x0011,
    RG16Snorm          = 0x0012,
    RG16Uint           = 0x0013,
    RG16Sint           = 0x0014,
    RG16Float          = 0x0015,
    RGBA8Unorm         = 0x0016,
    RGBA8UnormSrgb     = 0x0017,
    RGBA8Snorm         = 0x0018,
    RGBA8Uint          = 0x0019,
    RGBA8Sint          = 0x001A,
    BGRA8Unorm         = 0x001B,
    BGRA8UnormSrgb     = 0x001C,
    RGB10A2Uint        = 0x001D,
    RGB10A2Unorm       = 0x001E,
    RG11B10Ufloat      = 0x001F,
    RGB9E5Ufloat       = 0x0020,
    RG32Float          = 0x0021,
    RG32Uint           = 0x0022,
    RG32Sint           = 0x0023,
    RGBA16Unorm        = 0x0024,
    RGBA16Snorm        = 0x0025,
    RGBA16Uint         = 0x0026,
    RGBA16Sint         = 0x0027,
    RGBA16Float        = 0x0028,
    RGBA32Float        = 0x0029,
    RGBA32Uint         = 0x002A,
    RGBA32Sint         = 0x002B,
    Stencil8           = 0x002C,
    Depth16Unorm       = 0x002D,
    Depth24Plus        = 0x002E,
    Depth24PlusStencil8= 0x002F,
    Depth32Float       = 0x0030,
    Depth32FloatStencil8=0x0031,
    BC1RGBAUnorm       = 0x0032,
    BC1RGBAUnormSrgb   = 0x0033,
    BC2RGBAUnorm       = 0x0034,
    BC2RGBAUnormSrgb   = 0x0035,
    BC3RGBAUnorm       = 0x0036,
    BC3RGBAUnormSrgb   = 0x0037,
    BC4RUnorm          = 0x0038,
    BC4RSnorm          = 0x0039,
    BC5RGUnorm         = 0x003A,
    BC5RGSnorm         = 0x003B,
    BC6HRGBUfloat      = 0x003C,
    BC6HRGBFloat       = 0x003D,
    BC7RGBAUnorm       = 0x003E,
    BC7RGBAUnormSrgb   = 0x003F,
    ETC2RGB8Unorm      = 0x0040,
    ETC2RGB8UnormSrgb  = 0x0041,
    ETC2RGB8A1Unorm    = 0x0042,
    ETC2RGB8A1UnormSrgb= 0x0043,
    ETC2RGBA8Unorm     = 0x0044,
    ETC2RGBA8UnormSrgb = 0x0045,
    EACR11Unorm        = 0x0046,
    EACR11Snorm        = 0x0047,
    EACRG11Unorm       = 0x0048,
    EACRG11Snorm       = 0x0049,
    ASTC4x4Unorm       = 0x004A,
    ASTC4x4UnormSrgb   = 0x004B,
    ASTC5x4Unorm       = 0x004C,
    ASTC5x4UnormSrgb   = 0x004D,
    ASTC5x5Unorm       = 0x004E,
    ASTC5x5UnormSrgb   = 0x004F,
    ASTC6x5Unorm       = 0x0050,
    ASTC6x5UnormSrgb   = 0x0051,
    ASTC6x6Unorm       = 0x0052,
    ASTC6x6UnormSrgb   = 0x0053,
    ASTC8x5Unorm       = 0x0054,
    ASTC8x5UnormSrgb   = 0x0055,
    ASTC8x6Unorm       = 0x0056,
    ASTC8x6UnormSrgb   = 0x0057,
    ASTC8x8Unorm       = 0x0058,
    ASTC8x8UnormSrgb   = 0x0059,
    ASTC10x5Unorm      = 0x005A,
    ASTC10x5UnormSrgb  = 0x005B,
    ASTC10x6Unorm      = 0x005C,
    ASTC10x6UnormSrgb  = 0x005D,
    ASTC10x8Unorm      = 0x005E,
    ASTC10x8UnormSrgb  = 0x005F,
    ASTC10x10Unorm     = 0x0060,
    ASTC10x10UnormSrgb = 0x0061,
    ASTC12x10Unorm     = 0x0062,
    ASTC12x10UnormSrgb = 0x0063,
    ASTC12x12Unorm     = 0x0064,
    ASTC12x12UnormSrgb = 0x0065,
};

class Texture
{
    public:
        Texture();
        ~Texture();

        void LoadTexture(const std::string& path, TextureFormat format);
        void LoadTexture(uint8_t* pixels, size_t length, int width, int height, TextureFormat format);

        wgpu::Texture GetTexture() { return m_Texture; }
        wgpu::TextureView GetTextureView() { return m_View; }

        int GetHeight() { return m_Height; }
        int GetWidth() { return m_Width; }

    private:
        wgpu::Texture m_Texture;
        wgpu::TextureView m_View;
        int m_Width, m_Height;
};