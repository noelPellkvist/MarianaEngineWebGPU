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
    glm::vec2 uv;
};

void Init();

uint32_t ceilToNextMultiple(uint32_t value);