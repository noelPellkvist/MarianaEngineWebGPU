#include <Mariana/Graphics/Mesh.hpp>
#include "Graphics/Init.hpp"
#include <webgpu/webgpu_cpp.h>
#include <cstring>

struct IMesh::Impl
{
    wgpu::Buffer vertexBuffer;
    wgpu::Buffer indexBuffer;
};

IMesh::IMesh() : impl(std::make_unique<Impl>())
{}

IMesh::~IMesh() = default;

void IMesh::_buildMesh(void* vertices, size_t verticesLength, size_t vertexSize, void* indices, size_t indicesLength, size_t indexSize)
{
    m_VertexCount = verticesLength;
    m_IndexCount = indicesLength;
    m_VertexSize = vertexSize;
    // Without a GPU (a console test) only the CPU side is made.
    if (!device)
        return;

    // Mapped at creation: the data goes straight into the new buffer instead of through a queue
    // staging copy. Mapped sizes must be 4-byte aligned; only the real data is copied.
    auto createBuffer = [](const void* data, size_t size, wgpu::BufferUsage usage) {
        wgpu::BufferDescriptor bufferDesc;
        bufferDesc.size = (size + 3) & ~size_t(3);
        bufferDesc.usage = usage;
        bufferDesc.mappedAtCreation = true;
        wgpu::Buffer buffer = device.CreateBuffer(&bufferDesc);
        if (size > 0)
            std::memcpy(buffer.GetMappedRange(), data, size);
        buffer.Unmap();
        return buffer;
    };

    const wgpu::BufferUsage vertexUsage = m_Usage == MeshUsage::Dynamic ? wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst : wgpu::BufferUsage::Vertex;
    impl->vertexBuffer = createBuffer(vertices, verticesLength * vertexSize, vertexUsage);
    impl->indexBuffer = createBuffer(indices, indicesLength * indexSize, wgpu::BufferUsage::Index);
}

bool IMesh::WriteVertices(const void* vertices, size_t count, size_t vertexSize, const Bounds& bounds)
{
    if (m_Usage != MeshUsage::Dynamic || count != m_VertexCount || vertexSize != m_VertexSize)
        return false;
    // Queue writes must be a whole number of 4 bytes, as vertices are.
    if (impl->vertexBuffer && count > 0 && (count * vertexSize) % 4 == 0)
        device.GetQueue().WriteBuffer(impl->vertexBuffer, 0, vertices, count * vertexSize);
    if (!bounds.IsEmpty())
    {
        boundsMin = bounds.min;
        boundsMax = bounds.max;
        hasBounds = true;
    }
    ++deformVersion;
    return true;
}

void* IMesh::GetVertexBuffer()
{
    return &impl->vertexBuffer;
}

void* IMesh::GetIndexBuffer()
{
    return &impl->indexBuffer;
}