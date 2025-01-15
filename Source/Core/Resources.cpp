#include "Resources.h"
#include "Application.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#include <emscripten/bind.h>
#endif
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "../External/tiny_gltf.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "../External/tiny_obj_loader.h"


wgpu::ShaderModule Resources::LoadShader(const std::string& path)
{
    std::string shaderSource = LoadRawString(path);

    wgpu::ShaderModuleWGSLDescriptor wgslDesc{};
  wgslDesc.code = shaderSource.c_str();

  wgpu::ShaderModuleDescriptor shaderModuleDescriptor{
      .nextInChain = &wgslDesc};

    return device.CreateShaderModule(&shaderModuleDescriptor);
}

std::string Resources::LoadRawString(const std::string& path)
{
    std::filesystem::path realPath = std::string(RESOURCE_DIR) + path;
    std::cout << realPath << std::endl;
    std::ifstream file(realPath);
    if (!file.is_open()) {
        return nullptr;
    }
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    std::string Source(size, ' ');
    file.seekg(0);
    file.read(Source.data(), size);
    return Source;
}

wgpu::TextureView Resources::LoadTexture(const std::string& name)
{
    using namespace wgpu;

    std::string fullpath = std::string(RESOURCE_DIR) + "/Textures/" + name;
    std::cout << fullpath << std::endl;
    int width, height, channels;
    
    unsigned char* imageData = stbi_load(fullpath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (imageData == nullptr) {
        // Handle error if image loading failed
        std::cerr << "Failed to load texture: " << fullpath << std::endl;
        return {};
    }

    std::vector<uint8_t> pixels(4 * width * height);
    std::memcpy(pixels.data(), imageData, pixels.size());
    stbi_image_free(imageData);

    //Create Texture
    TextureFormat textureFormat = TextureFormat::RGBA8UnormSrgb;
    TextureDescriptor textureDesc;
    textureDesc.dimension = TextureDimension::e2D;
    textureDesc.format = textureFormat;
    textureDesc.mipLevelCount = 1;
    textureDesc.sampleCount = 1;
    textureDesc.size = {static_cast<unsigned int>(width), static_cast<unsigned int>(height), 1};
    textureDesc.usage = TextureUsage::TextureBinding | TextureUsage::CopyDst;
    textureDesc.viewFormatCount = 1;
    textureDesc.viewFormats = &textureFormat;
    Texture texture = device.CreateTexture(&textureDesc);

    TextureViewDescriptor textureViewDesc;
    textureViewDesc.aspect = TextureAspect::All;
    textureViewDesc.baseArrayLayer = 0;
    textureViewDesc.arrayLayerCount = 1;
    textureViewDesc.baseMipLevel = 0;
    textureViewDesc.mipLevelCount = 1;
    textureViewDesc.dimension = TextureViewDimension::e2D;
    textureViewDesc.format = textureFormat;
    TextureView textureView = texture.CreateView(&textureViewDesc);



	ImageCopyTexture destination;
	destination.texture = texture;
	destination.mipLevel = 0;
	destination.origin = { 0, 0, 0 };
	destination.aspect = TextureAspect::All;

	TextureDataLayout source;
	source.offset = 0;
	source.bytesPerRow = 4 * textureDesc.size.width;
	source.rowsPerImage = textureDesc.size.height;

    device.GetQueue().WriteTexture(&destination, pixels.data(), pixels.size(), &source, &textureDesc.size);

    return textureView;
}

wgpu::TextureView Resources::GetEmptyTexture()
{
    static wgpu::TextureView cachedTextureView;
    static bool isInitialized = false;

    if (!isInitialized) {
        using namespace wgpu;
        std::vector<uint8_t> pixels = {1,0,0,1};

        // Create Texture
        TextureFormat textureFormat = TextureFormat::RGBA8UnormSrgb;
        TextureDescriptor textureDesc;
        textureDesc.dimension = TextureDimension::e2D;
        textureDesc.format = textureFormat;
        textureDesc.mipLevelCount = 1;
        textureDesc.sampleCount = 1;
        textureDesc.size = {static_cast<unsigned int>(1), static_cast<unsigned int>(1), 1};
        textureDesc.usage = TextureUsage::TextureBinding | TextureUsage::CopyDst;
        textureDesc.viewFormatCount = 1;
        textureDesc.viewFormats = &textureFormat;
        Texture texture = device.CreateTexture(&textureDesc);

        TextureViewDescriptor textureViewDesc;
        textureViewDesc.aspect = TextureAspect::All;
        textureViewDesc.baseArrayLayer = 0;
        textureViewDesc.arrayLayerCount = 1;
        textureViewDesc.baseMipLevel = 0;
        textureViewDesc.mipLevelCount = 1;
        textureViewDesc.dimension = TextureViewDimension::e2D;
        textureViewDesc.format = textureFormat;
        cachedTextureView = texture.CreateView(&textureViewDesc);

        ImageCopyTexture destination;
        destination.texture = texture;
        destination.mipLevel = 0;
        destination.origin = {0, 0, 0};
        destination.aspect = TextureAspect::All;

        TextureDataLayout source;
        source.offset = 0;
        source.bytesPerRow = 4 * textureDesc.size.width;
        source.rowsPerImage = textureDesc.size.height;

        device.GetQueue().WriteTexture(&destination, pixels.data(), pixels.size(), &source, &textureDesc.size);

        isInitialized = true;
    }

    return cachedTextureView;
}

std::vector<wgpu::TextureView> Resources::LoadTextures(/*tinygltf::Image& img*/)
{
    std::string fullpath = std::string(RESOURCE_DIR) + "/" + "DamagedHelmet.glb";
    std::vector<wgpu::TextureView> result;
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, fullpath);

    if (!warn.empty()) {
      printf("Warn: %s\n", warn.c_str());
    }

    if (!err.empty()) {
      printf("Err: %s\n", err.c_str());
    }

    if (!ret) {
      printf("Failed to parse glTF\n");
      return {};
    }
    for(int i = 0; i < model.images.size(); i++)
    {
        tinygltf::Image img = model.images[i];

        using namespace wgpu;
        int width = img.width;
        int height = img.height;
        int channels = img.component;

        unsigned char* imageData = img.image.data();

        if (imageData == nullptr) {
            // Handle error if image loading failed
            std::cerr << "Failed to load texture from gltf model!" << std::endl;
            return {};
        }
        std::vector<uint8_t> pixels(channels * width * height);
        std::memcpy(pixels.data(), imageData, pixels.size());


        //Create Texture
        TextureFormat textureFormat = TextureFormat::RGBA8Unorm;
        TextureDescriptor textureDesc;
        textureDesc.dimension = TextureDimension::e2D;
        textureDesc.format = textureFormat;
        textureDesc.mipLevelCount = 1;
        textureDesc.sampleCount = 1;
        textureDesc.size = {static_cast<unsigned int>(width), static_cast<unsigned int>(height), 1};
        textureDesc.usage = TextureUsage::TextureBinding | TextureUsage::CopyDst;
        textureDesc.viewFormatCount = 1;
        textureDesc.viewFormats = &textureFormat;
        Texture texture = device.CreateTexture(&textureDesc);

        TextureViewDescriptor textureViewDesc;
        textureViewDesc.aspect = TextureAspect::All;
        textureViewDesc.baseArrayLayer = 0;
        textureViewDesc.arrayLayerCount = 1;
        textureViewDesc.baseMipLevel = 0;
        textureViewDesc.mipLevelCount = 1;
        textureViewDesc.dimension = TextureViewDimension::e2D;
        textureViewDesc.format = textureFormat;
        TextureView textureView = texture.CreateView(&textureViewDesc);



	    ImageCopyTexture destination;
	    destination.texture = texture;
	    destination.mipLevel = 0;
	    destination.origin = { 0, 0, 0 };
	    destination.aspect = TextureAspect::All;

	    TextureDataLayout source;
	    source.offset = 0;
	    source.bytesPerRow = 4 * textureDesc.size.width;
	    source.rowsPerImage = textureDesc.size.height;

        device.GetQueue().WriteTexture(&destination, pixels.data(), pixels.size(), &source, &textureDesc.size);
        result.push_back(textureView);
    }
    return result;
}

wgpu::TextureView Resources::CreateEmptyTexture(int width, int height, wgpu::TextureFormat format)
{
    using namespace wgpu;
    TextureDescriptor textureDesc;
    textureDesc.dimension = TextureDimension::e2D;
    textureDesc.format = format;
    textureDesc.mipLevelCount = 1;
    textureDesc.sampleCount = 1;
    textureDesc.size = {static_cast<unsigned int>(width), static_cast<unsigned int>(height), 1};
    textureDesc.usage = TextureUsage::TextureBinding | TextureUsage::RenderAttachment;
    textureDesc.viewFormatCount = 1;
    textureDesc.viewFormats = &format;
    Texture texture = device.CreateTexture(&textureDesc);

    TextureViewDescriptor textureViewDesc;
    textureViewDesc.aspect = TextureAspect::All;
    textureViewDesc.baseArrayLayer = 0;
    textureViewDesc.arrayLayerCount = 1;
    textureViewDesc.baseMipLevel = 0;
    textureViewDesc.mipLevelCount = 1;
    textureViewDesc.dimension = TextureViewDimension::e2D;
    textureViewDesc.format = format;
    TextureView textureView = texture.CreateView(&textureViewDesc);

    return textureView;
}
