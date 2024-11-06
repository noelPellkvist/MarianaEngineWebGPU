#include "Mesh.hpp"

#include <iostream>

Mesh::Mesh()
{
    vertices = {
        {glm::vec2(-0.5, -0.5)},
        {glm::vec2(0.5, -0.5)},
        {glm::vec2(0.0, 0.5)},

        {glm::vec2(-0.55f, -0.5)},
        {glm::vec2(-0.05f, 0.5)},
        {glm::vec2(-0.55f, 0.5)}
    };
}

void Mesh::BuildMesh()
{
    

    wgpu::BufferDescriptor bufferDesc;
    bufferDesc.size = vertices.size() * sizeof(Vertex);
    bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex;
    bufferDesc.mappedAtCreation = false;
    vertexBuffer = device.CreateBuffer(&bufferDesc);

    device.GetQueue().WriteBuffer(vertexBuffer, 0, vertices.data(), bufferDesc.size); 
}

Mesh::~Mesh()
{
    
}