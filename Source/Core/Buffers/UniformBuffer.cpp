#include "UniformBuffer.hpp"
#include "../GlobalVaribles.hpp"
#include <gtc/matrix_transform.hpp>

UniformBufferData InitUBO()
{
    UniformBufferData res;

    wgpu::Buffer& uboBuffer = res.uniformBuffer;
    wgpu::BufferDescriptor bufferDesc;
    bufferDesc.size = sizeof(UBO);
    bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
    bufferDesc.mappedAtCreation = false;
    uboBuffer = device.CreateBuffer(&bufferDesc);

    UBO ubo;

    float aspect = static_cast<float>(1336) / static_cast<float>(768);
    ubo.projectionMatrix = glm::perspective(glm::radians(60.0f), aspect, 0.01f, 100.0f);
    float currentTime = 0;
    ubo.viewMatrix = glm::lookAt(glm::vec3(15 * glm::sin(currentTime), 0, 15 * glm::cos(currentTime)), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    device.GetQueue().WriteBuffer(uboBuffer, 0, &ubo, sizeof(UBO));
    res.data = ubo;

    std::vector<wgpu::BindGroupLayoutEntry> globalBindingLayouts(1);
    globalBindingLayouts[0] = {};
    globalBindingLayouts[0].binding = 0;
    globalBindingLayouts[0].visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    globalBindingLayouts[0].buffer.type = wgpu::BufferBindingType::Uniform;
    globalBindingLayouts[0].buffer.minBindingSize = sizeof(UBO);

    wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc{};
    bindGroupLayoutDesc.entryCount = (uint32_t)globalBindingLayouts.size();
    bindGroupLayoutDesc.entries = globalBindingLayouts.data();
    res.bindGroupLayout = device.CreateBindGroupLayout(&bindGroupLayoutDesc);

    std::vector<wgpu::BindGroupEntry> bindings(1);

    bindings[0] = {};
    bindings[0].binding = 0;
    bindings[0].buffer = res.uniformBuffer;
    bindings[0].offset = 0;
    bindings[0].size = sizeof(UBO);

    wgpu::BindGroupDescriptor bindGroupDesc{};
    bindGroupDesc.layout = res.bindGroupLayout;
    bindGroupDesc.entryCount = (uint32_t)bindings.size();
    bindGroupDesc.entries = bindings.data();
    res.bindGroup = device.CreateBindGroup(&bindGroupDesc);
    return res;
}