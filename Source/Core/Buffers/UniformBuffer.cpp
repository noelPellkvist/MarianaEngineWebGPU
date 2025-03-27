#include "UniformBuffer.hpp"
#include "../GlobalVaribles.hpp"


UniformBuffer CreateUniformBuffer(void* data, size_t dataSize, bool isDynamic)
{
    UniformBuffer res;

    wgpu::SupportedLimits supportedLimits;
    device.GetLimits(&supportedLimits);
    wgpu::Limits deviceLimits = supportedLimits.limits;

    res.uniformStride = ceilToNextMultiple(
        (uint32_t)dataSize,
        (uint32_t)deviceLimits.minUniformBufferOffsetAlignment
    );

    wgpu::Buffer& uboBuffer = res.uniformBuffer;
    wgpu::BufferDescriptor bufferDesc;
    if (!isDynamic)
        bufferDesc.size = dataSize;
    else 
        bufferDesc.size = res.uniformStride * 100;
    bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
    bufferDesc.mappedAtCreation = false;
    uboBuffer = device.CreateBuffer(&bufferDesc);

    device.GetQueue().WriteBuffer(uboBuffer, 0, data, dataSize);

    

    std::vector<wgpu::BindGroupLayoutEntry> globalBindingLayouts(1);
    globalBindingLayouts[0] = {};
    globalBindingLayouts[0].binding = 0;
    globalBindingLayouts[0].visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    globalBindingLayouts[0].buffer.type = wgpu::BufferBindingType::Uniform;
    globalBindingLayouts[0].buffer.hasDynamicOffset = isDynamic;
    globalBindingLayouts[0].buffer.minBindingSize = dataSize;

    wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc{};
    bindGroupLayoutDesc.entryCount = (uint32_t)globalBindingLayouts.size();
    bindGroupLayoutDesc.entries = globalBindingLayouts.data();
    res.bindGroupLayout = device.CreateBindGroupLayout(&bindGroupLayoutDesc);

    std::vector<wgpu::BindGroupEntry> bindings(1);

    bindings[0] = {};
    bindings[0].binding = 0;
    bindings[0].buffer = res.uniformBuffer;
    bindings[0].offset = 0;
    bindings[0].size = dataSize;

    wgpu::BindGroupDescriptor bindGroupDesc{};
    bindGroupDesc.layout = res.bindGroupLayout;
    bindGroupDesc.entryCount = (uint32_t)bindings.size();
    bindGroupDesc.entries = bindings.data();
    res.bindGroup = device.CreateBindGroup(&bindGroupDesc);
    return res;
}


uint32_t ceilToNextMultiple(uint32_t value, uint32_t step) {
    uint32_t divide_and_ceil = value / step + (value % step == 0 ? 0 : 1);
    return step * divide_and_ceil;
}

void UniformBuffer::UpdateValue(void* data, size_t dataSize, uint32_t index)
{
    device.GetQueue().WriteBuffer(uniformBuffer, uniformStride * index, data, dataSize);
}