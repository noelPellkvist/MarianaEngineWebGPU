#include "Resources.h"
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#include <emscripten/bind.h>
#endif

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