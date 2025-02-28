#pragma once

#include <webgpu/webgpu_cpp.h>

struct Submesh
{
    uint32_t startVertex;
    uint32_t vertexCount;
    uint32_t startIndex;
    uint32_t indexxCount;
};

struct Mesh
{
    std::vector<Submesh> submeshes;
    wgpu::Buffer vertexBuffer;
    wgpu::Buffer indexBuffer;
    size_t indexCount;
};