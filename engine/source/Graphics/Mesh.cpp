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

    impl->vertexBuffer = createBuffer(vertices, verticesLength * vertexSize, wgpu::BufferUsage::Vertex);
    impl->indexBuffer = createBuffer(indices, indicesLength * indexSize, wgpu::BufferUsage::Index);
}

void* IMesh::GetVertexBuffer()
{
    return &impl->vertexBuffer;
}

void* IMesh::GetIndexBuffer()
{
    return &impl->indexBuffer;
}