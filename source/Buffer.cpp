#include <webgpu/webgpu_cpp.h>
#include <Buffer.hpp>
#include "Init.hpp"

struct Buffer::Impl
{
    wgpu::Buffer m_Buffer;
};

Buffer::Buffer(UniformBufferLayout layout)
    : _impl(std::make_shared<Impl>()), m_Layout(layout), m_Type(BufferType::Uniform)
{
    m_Size = layout.total_size();
    m_IsDynamic = layout.IsDynamic();
}

Buffer::Buffer(StorageBufferLayout layout)
    : _impl(std::make_shared<Impl>()), m_Layout(layout), m_Type(BufferType::Storage)
{
    m_Size = layout.total_size();
    m_IsDynamic = layout.IsDynamic();
}

Buffer::Buffer(StorageArrayLayout layout)
    : _impl(std::make_shared<Impl>()), m_Layout(layout), m_Type(BufferType::Storage)
{
    m_Size = layout.total_size();
    m_IsDynamic = layout.IsDynamic();
}

uint64_t Buffer::GetStride() const
{
    return std::visit([](const auto& layout) -> uint64_t {
        if constexpr (std::is_same_v<std::decay_t<decltype(layout)>, UniformBufferLayout>) {
            return layout.GetUniformStride();
        } else {
            return layout.GetStorageStride();
        }
    }, m_Layout);
}

void Buffer::Build()
{
    wgpu::BufferDescriptor bufferDesc;
    bufferDesc.usage = wgpu::BufferUsage::CopyDst | (m_Type == BufferType::Uniform
        ? wgpu::BufferUsage::Uniform
        : wgpu::BufferUsage::Storage);
    bufferDesc.mappedAtCreation = false;
    bufferDesc.size = m_IsDynamic ? GetStride() * 1024 : m_Size;
    _impl->m_Buffer = device.CreateBuffer(&bufferDesc);
}

void Buffer::WriteRaw(const void* data, uint64_t size, uint64_t index)
{
    WriteRawBytes(data, size, index * GetBindingSize());
}

void Buffer::WriteRawBytes(const void* data, uint64_t size, uint64_t byteOffset)
{
    const uint64_t capacity = m_IsDynamic ? GetStride() * 1024 : m_Size;
    if (byteOffset + size > capacity) {
        throw std::runtime_error("Buffer::WriteRawBytes: write exceeds buffer size");
    }
    wgpu::Queue queue = device.GetQueue();
    queue.WriteBuffer(_impl->m_Buffer, byteOffset, data, size);
}

const void* Buffer::GetBuffer() const
{
    return &_impl->m_Buffer;
}





