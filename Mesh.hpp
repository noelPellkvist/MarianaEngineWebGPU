#pragma once
#include "GlobalVaribles.hpp"
#include <glm.hpp>
#include <webgpu/webgpu_cpp.h>
#include <vector>

class Mesh 
{
    

    public:

    struct Vertex {
        glm::vec2 position;
        glm::vec3 normal;
    };

    Mesh();
    ~Mesh();

    void BuildMesh();

    uint32_t getIndexCount() { return indices.size(); }

    const wgpu::Buffer& GetVertexBuffer() { return vertexBuffer; }
    const wgpu::Buffer& GetIndexBuffer() { return indexBuffer; }

    private:
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;

    wgpu::Buffer vertexBuffer;
    wgpu::Buffer indexBuffer;
};