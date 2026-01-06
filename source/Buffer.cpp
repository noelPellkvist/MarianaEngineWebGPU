#include <webgpu/webgpu_cpp.h>
#include <Buffer.hpp> 
#include "Init.hpp"

struct Buffer::Impl
{
    wgpu::Buffer m_Buffer;
};

Buffer::Buffer(UniformBufferLayout layout)
    : _impl(std::make_shared<Impl>()), m_Layout(layout)
{
    m_Size = layout.total_size();
    m_IsDynamic = layout.IsDynamic();
}

void Buffer::Build()
{
    wgpu::BufferDescriptor bufferDesc;
    bufferDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    bufferDesc.mappedAtCreation = false;
    if(m_IsDynamic)
    {
        bufferDesc.size = m_Layout.GetUniformStride() * 256;
    }
    else
        bufferDesc.size = m_Size;
    _impl->m_Buffer = device.CreateBuffer(&bufferDesc);
}

void Buffer::WriteRaw(const void* data, uint64_t size, uint64_t index)
{
    wgpu::Queue queue = device.GetQueue();
    queue.WriteBuffer(_impl->m_Buffer, index * GetBindingSize(), data, size);
}

const void* Buffer::GetBuffer() const
{
    return &_impl->m_Buffer;
}