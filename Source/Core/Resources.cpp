#include "Resources.hpp"


#define TINYOBJLOADER_IMPLEMENTATION
#include "../External/tiny_obj_loader.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <glm.hpp>
#include "GlobalVaribles.hpp"

std::string Resources::LoadString(const std::string& path)
{
    std::filesystem::path realPath = std::string(RESOURCE_DIR) + "/" + path;
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

Mesh Resources::LoadObjMesh(const std::string& path, const Shader& shader)
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
    std::vector<uint16_t> indices;
    std::vector<glm::vec3> positions;

    size_t i = 0;

    for (const auto& shape : shapes)
    {
        for (const auto& index : shape.mesh.indices)
        {
            positions.push_back({
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2]
			});
            indices.push_back(i);
            i++;
        }
    }
    Mesh m;
    Submesh subMesh;
    subMesh.indexxCount = i;
    subMesh.startIndex = 0;
    subMesh.startVertex = 0;
    subMesh.vertexCount = i;

    m.submeshes = {};
    m.submeshes.push_back(subMesh);
    m.indexCount = i;

    std::vector<VertexAttribute> attributes = 
    {
        {"POSITION", positions.data(), positions.size() }
    };
    m.vertexBuffer = shader.CreateVertexBuffer(attributes);

    wgpu::BufferDescriptor bufferDesc;
    bufferDesc.mappedAtCreation = false;
    bufferDesc.size = indices.size() * sizeof(uint16_t);
    bufferDesc.size = (bufferDesc.size + 3) & ~3;
    bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Index;

    m.indexBuffer = device.CreateBuffer(&bufferDesc);
    device.GetQueue().WriteBuffer(m.indexBuffer, 0, indices.data(), bufferDesc.size);
    m.indexCount = indices.size();
    m.shaderIndex = 0;
    return m;
}