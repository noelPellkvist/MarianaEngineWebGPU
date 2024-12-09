#include "Resources.h"
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


#include "tiny_gltf.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"


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

Mesh Resources::LoadOBJMesh(const std::string& path)
{
    std::string str = LoadRawString(path);
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

Mesh LoadGLTFPrimitives(tinygltf::Model& model)
{
    std::vector<Mesh::Vertex> vertexData;
    std::vector<uint16_t> indices;

    for (const auto& primitive : model.meshes[0].primitives) {
        // Extract position data
        std::vector<glm::vec3> positions;
        if (primitive.attributes.find("POSITION") != primitive.attributes.end()) {
            int posAccessorIndex = primitive.attributes.at("POSITION");
            const tinygltf::Accessor& posAccessor = model.accessors[posAccessorIndex];
            const tinygltf::BufferView& posBufferView = model.bufferViews[posAccessor.bufferView];
            const tinygltf::Buffer& posBuffer = model.buffers[posBufferView.buffer];

            const float* posData = reinterpret_cast<const float*>(&posBuffer.data[posBufferView.byteOffset]);
            size_t numVertices = posAccessor.count;
            vertexData.reserve(numVertices);
            for (size_t i = 0; i < numVertices; ++i) {
                positions.push_back(glm::vec3(posData[i * 3 + 0], posData[i * 3 + 1], posData[i * 3 + 2]));
            }
        }

        std::cout << "Loading positions" << std::endl;

        std::vector<glm::vec3> normals;
        if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
            int normalAccessorIndex = primitive.attributes.at("NORMAL");
            const tinygltf::Accessor& normalAccessor = model.accessors[normalAccessorIndex];
            const tinygltf::BufferView& normalBufferView = model.bufferViews[normalAccessor.bufferView];
            const tinygltf::Buffer& normalBuffer = model.buffers[normalBufferView.buffer];

            const float* normalData = reinterpret_cast<const float*>(&normalBuffer.data[normalBufferView.byteOffset]);
            size_t numNormals = normalAccessor.count;

            for (size_t i = 0; i < numNormals; ++i) {
                normals.push_back(glm::vec3(normalData[i * 3 + 0], normalData[i * 3 + 1], normalData[i * 3 + 2]));
            }
        }

        std::cout << "Loading normals" << std::endl;

        std::vector<glm::vec2> uvs;
        if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
            int uvAccessorIndex = primitive.attributes.at("TEXCOORD_0");
            const tinygltf::Accessor& uvAccessor = model.accessors[uvAccessorIndex];
            const tinygltf::BufferView& uvBufferView = model.bufferViews[uvAccessor.bufferView];
            const tinygltf::Buffer& uvBuffer = model.buffers[uvBufferView.buffer];

            const float* uvData = reinterpret_cast<const float*>(&uvBuffer.data[uvBufferView.byteOffset]);
            size_t numUVs = uvAccessor.count;

            for (size_t i = 0; i < numUVs; ++i) {
                uvs.push_back(glm::vec2(uvData[i * 2 + 0], uvData[i * 2 + 1] - 1));
            }
        }
        std::cout << "Loading uvs" << std::endl;

        std::vector<glm::vec4> colors;
        if (primitive.attributes.find("COLOR_0") != primitive.attributes.end()) {
            int colorAccessorIndex = primitive.attributes.at("COLOR_0");
            const tinygltf::Accessor& colorAccessor = model.accessors[colorAccessorIndex];
            const tinygltf::BufferView& colorBufferView = model.bufferViews[colorAccessor.bufferView];
            const tinygltf::Buffer& colorBuffer = model.buffers[colorBufferView.buffer];

            const float* colorData = reinterpret_cast<const float*>(&colorBuffer.data[colorBufferView.byteOffset]);
            size_t numColors = colorAccessor.count;

            for (size_t i = 0; i < numColors; ++i) {
                colors.push_back(glm::vec4(colorData[i * 4 + 0], colorData[i * 4 + 1], colorData[i * 4 + 2], colorData[i * 4 + 3]));
            }
        }

        std::cout << "Loading colors" << std::endl;

        size_t numVertices = positions.size();
        if (normals.size() != numVertices || uvs.size() != numVertices) {
            std::cerr << "Error: Mismatch in number of positions, normals, or UVs\n";
            return {};
        }
        else
            std::cout << "Mesh created succesfully" << std::endl;

        for (size_t i = 0; i < numVertices; ++i) {
            Mesh::Vertex v = {};
            v.position = positions[i];
            v.normal = normals[i];
            if(colors.size() > 0)
                v.color = colors[i];
            v.uv = uvs[i];
            vertexData.push_back(v);
        }
        
        if (primitive.indices > -1) {
            int indicesAccessorIndex = primitive.indices;
            const tinygltf::Accessor& indicesAccessor = model.accessors[indicesAccessorIndex];
            const tinygltf::BufferView& indicesBufferView = model.bufferViews[indicesAccessor.bufferView];
            const tinygltf::Buffer& indicesBuffer = model.buffers[indicesBufferView.buffer];

            if (indicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                const uint16_t* indicesData = reinterpret_cast<const uint16_t*>(&indicesBuffer.data[indicesBufferView.byteOffset]);
                size_t numIndices = indicesAccessor.count;

                for (size_t i = 0; i < numIndices; ++i) {
                    indices.push_back(indicesData[i]);
                }
            } else {
                std::cerr << "Unsupported index component type: " << indicesAccessor.componentType << std::endl;
            }
        }   
    }
    Mesh newMesh(vertexData, indices);
    newMesh.BuildMesh();
    return newMesh;
}

std::vector<wgpu::TextureView> LoadGLTFTextures(tinygltf::Model& model)
{
    for(auto& t : model.images)
        std::cout << "Model has texture named: " << t.name << std::endl;

    return {};
}

GameObject Resources::LoadGLTFMesh(const std::string& path)
{
    std::string fullpath = std::string(RESOURCE_DIR) + "/" + path;

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
      return GameObject();
    }

    std::cout << "Succesfully loaded GLTF: " << model.meshes[0].name << std::endl;
    
    Mesh newMesh = LoadGLTFPrimitives(model);
    std::vector<wgpu::TextureView> textures = LoadGLTFTextures(model);
    
    return GameObject(model.meshes[0].name, newMesh);
}