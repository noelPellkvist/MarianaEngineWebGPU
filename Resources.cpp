#include "Resources.h"
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#include <emscripten/bind.h>
#endif
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


wgpu::ShaderModule Resources::LoadShader(const std::string& path)
{
    std::string shaderSource = LoadString(path);

    wgpu::ShaderModuleWGSLDescriptor wgslDesc{};
  wgslDesc.code = shaderSource.c_str();

  wgpu::ShaderModuleDescriptor shaderModuleDescriptor{
      .nextInChain = &wgslDesc};

    return device.CreateShaderModule(&shaderModuleDescriptor);
}

std::string Resources::LoadString(const std::string& path)
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

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
Mesh Resources::LoadOBJMesh(const std::string& path)
{
    std::string str = LoadString(path);
    std::istringstream stream(str);
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string warn;
    std::string err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, &stream)) {
				throw std::runtime_error(warn + err);
			}

    std::vector<Mesh::Vertex> vertices;
    std::vector<uint16_t> indices;
    int i = 0;

    for (const auto& shape : shapes) 
    {
		for (const auto& index : shape.mesh.indices) 
        {
            Mesh::Vertex vertex;

            vertex.position = {
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2]
			};
					
		    vertex.normal = {
				attrib.normals[3 * index.normal_index + 0],
				attrib.normals[3 * index.normal_index + 1],
				attrib.normals[3 * index.normal_index + 2]
			};

            vertex.color = { 1.0f, 1.0f, 1.0f };

			vertex.uv = {
				attrib.texcoords[2 * index.texcoord_index + 0],
				1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
			};

            vertices.push_back(vertex);
            indices.push_back(i);
            i++;
        }
    }

    return Mesh(vertices, indices);
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

    return textureView;
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