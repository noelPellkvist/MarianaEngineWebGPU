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

    uint32_t getVertexCount() { return vertices.size(); }

    const wgpu::Buffer& GetVertexBuffer() { return vertexBuffer; }

    private:
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;

    wgpu::Buffer vertexBuffer;
};