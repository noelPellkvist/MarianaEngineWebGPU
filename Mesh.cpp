#include "Mesh.hpp"

#include <iostream>

Mesh::Mesh()
{
    vertices = {
        {glm::vec3(-0.5, -0.5, -0.3), glm::vec3(1,1,1)},
        {glm::vec3(0.5, -0.5, -0.3), glm::vec3(1,1,1)},
        {glm::vec3(0.5, 0.5, -0.3), glm::vec3(1,1,1)},
        {glm::vec3(-0.5, 0.5, -0.3), glm::vec3(1,1,1)},

        {glm::vec3(0.0, 0.0, 0.5), glm::vec3(0.5,0.5,0.5)}
    };

    indices = {
        0,1,2,
        0,2,3,
        0,1,4,
        1,2,4,
        2,3,4,
        3,0,4
    };
}

Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint16_t>& indices) : vertices(vertices), indices(indices)
{
}

void Mesh::BuildMesh()
{
    

    wgpu::BufferDescriptor bufferDesc;
    bufferDesc.size = vertices.size() * sizeof(Vertex);
    bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex;
    bufferDesc.mappedAtCreation = false;
    vertexBuffer = device.CreateBuffer(&bufferDesc);
    device.GetQueue().WriteBuffer(vertexBuffer, 0, vertices.data(), bufferDesc.size); 

    bufferDesc.size = indices.size() * sizeof(uint16_t);
    bufferDesc.size = (bufferDesc.size + 3) & ~3;
    bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Index;
    
    indexBuffer = device.CreateBuffer(&bufferDesc);
    device.GetQueue().WriteBuffer(indexBuffer, 0, indices.data(), bufferDesc.size); 
}

Mesh::~Mesh()
{
    
}