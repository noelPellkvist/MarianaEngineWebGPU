#include <Mesh.hpp>
#include <Init.hpp>
#include <webgpu/webgpu_cpp.h>

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
    wgpu::BufferDescriptor bufferDesc;
    bufferDesc.size = verticesLength * vertexSize;
    bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex; 
    bufferDesc.mappedAtCreation = false;
    impl->vertexBuffer = device.CreateBuffer(&bufferDesc);

    device.GetQueue().WriteBuffer(impl->vertexBuffer, 0, vertices, bufferDesc.size);

    bufferDesc.size = indicesLength * indexSize;
    bufferDesc.size = (bufferDesc.size + 3) & ~3;
    bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Index;
    impl->indexBuffer = device.CreateBuffer(&bufferDesc);
    device.GetQueue().WriteBuffer(impl->indexBuffer, 0, indices, bufferDesc.size); 
}

void* IMesh::GetVertexBuffer()
{
    return &impl->vertexBuffer;
}

void* IMesh::GetIndexBuffer()
{
    return &impl->indexBuffer;
}