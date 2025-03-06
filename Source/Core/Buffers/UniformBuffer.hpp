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

struct UniformBufferEntry
{
    std::vector<uint8_t> data;
    size_t actualSize;
    uint32_t offset;
};

struct UniformBuffer
{
    bool dynamic = false;
    std::vector<UniformBufferEntry> entries;
    std::vector<uint8_t> data;
    void BuildData();
    
};

uint32_t ceilToNextMultiple(uint32_t value, uint32_t step);

UniformBufferData InitUBO();

