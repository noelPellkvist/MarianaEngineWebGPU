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

Buffer::Buffer(RawBufferLayout layout)
    : _impl(std::make_shared<Impl>()), m_Layout(layout), m_Type(layout.type)
{
    m_Size = (layout.size + 15) / 16 * 16;
    m_IsDynamic = false;
}

uint64_t Buffer::GetStride() const
{
    return std::visit([this](const auto& layout) -> uint64_t {
        using LayoutT = std::decay_t<decltype(layout)>;
        if constexpr (std::is_same_v<LayoutT, UniformBufferLayout>) {
            return layout.GetUniformStride();
        } else if constexpr (std::is_same_v<LayoutT, RawBufferLayout>) {
            return m_Size;
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





