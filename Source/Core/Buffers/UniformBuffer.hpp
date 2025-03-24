#pragma once
#include <any>
#include <vector>
#include <webgpu/webgpu_cpp.h>
#include <glm.hpp>

struct UBO
{
    glm::mat4x4 projectionMatrix;
    glm::mat4x4 viewMatrix;
};

struct UniformBuffer
{
    wgpu::BindGroup bindGroup;
    wgpu::BindGroupLayout bindGroupLayout;
    wgpu::Buffer uniformBuffer;
    size_t dataSize;
    bool dynamic;
    uint32_t uniformStride;

    void UpdateValue(void* data, size_t dataSize, uint32_t index);
};

uint32_t ceilToNextMultiple(uint32_t value, uint32_t step);

UniformBuffer CreateUniformBuffer(void* data, size_t dataSize, bool isDynamic);

