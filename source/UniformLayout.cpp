#include <UniformLayout.hpp>
#include <webgpu/webgpu_cpp.h>
#include "Init.hpp"

struct IUniformLayout::Impl
{
    wgpu::BindGroupEntry m_BindgroupEntry{};
    wgpu::BindGroupLayoutEntry m_BindgroupLayoutEntry{};
    wgpu::Buffer m_GPUBuffer{};
};

IUniformLayout::IUniformLayout() : _impl(std::make_unique<Impl>())
{}

IUniformLayout::~IUniformLayout() = default;

void* IUniformLayout::GetBindGroupEntry()
{ 
    return &_impl->m_BindgroupEntry; 
}

void* IUniformLayout::GetBindGroupLayoutEntry() 
{ 
    return &_impl->m_BindgroupLayoutEntry; 
}

void IUniformLayout::WriteBuffer(uint64_t offset, void* data, size_t size)
{
    device.GetQueue().WriteBuffer(_impl->m_GPUBuffer, offset, data, size);
}

void IUniformLayout::Init(uint32_t bindingIndex)
{
    wgpu::BufferDescriptor bufferDesc;
    if(m_isDynamic)
    {
        uniformStride = ceilToNextMultiple(total_size_);
        bufferDesc.size = uniformStride * 256;
    }
    else
        bufferDesc.size = total_size_;
        
    bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
    bufferDesc.mappedAtCreation = false;
    _impl->m_GPUBuffer = device.CreateBuffer(&bufferDesc);

    WriteBuffer(0, m_Buffer.data(), m_Buffer.size());

    _impl->m_BindgroupLayoutEntry.binding = bindingIndex;
    _impl->m_BindgroupLayoutEntry.visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    _impl->m_BindgroupLayoutEntry.buffer.type = wgpu::BufferBindingType::Uniform;
    _impl->m_BindgroupLayoutEntry.buffer.minBindingSize = m_isDynamic ? uniformStride : total_size_;
    _impl->m_BindgroupLayoutEntry.buffer.hasDynamicOffset = m_isDynamic;

    _impl->m_BindgroupEntry.buffer = _impl->m_GPUBuffer;
    _impl->m_BindgroupEntry.offset = 0;
    _impl->m_BindgroupEntry.size = m_isDynamic ? uniformStride : total_size_;
    _impl->m_BindgroupEntry.binding = bindingIndex;
}