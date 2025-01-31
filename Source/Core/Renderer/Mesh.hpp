#pragma once

#include <webgpu/webgpu_cpp.h>

struct Mesh
{
    wgpu::Buffer vertexBuffer;
    wgpu::Buffer indexBuffer;
    size_t indexCount;
};