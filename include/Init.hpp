#pragma once
#include <webgpu/webgpu_cpp.h>
#include <glm/glm.hpp>

extern wgpu::Instance instance;
extern wgpu::Adapter adapter;
extern wgpu::Device device;

extern wgpu::Surface surface;
extern wgpu::TextureFormat windowFormat;

extern wgpu::Limits deviceLimits;

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
};

void Init();