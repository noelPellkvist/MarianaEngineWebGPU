#pragma once
#include <vector>
#include <webgpu/webgpu_cpp.h>
#include <glm.hpp>

struct UBO
{
    glm::mat4x4 projectionMatrix;
    glm::mat4x4 viewMatrix;
};

struct UniformBufferData
{
    wgpu::BindGroup bindGroup;
    wgpu::BindGroupLayout bindGroupLayout;
    wgpu::Buffer uniformBuffer;
    UBO data;
};

UniformBufferData InitUBO();

