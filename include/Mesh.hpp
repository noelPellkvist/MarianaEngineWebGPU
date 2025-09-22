#pragma once
#include <vector>
#include <webgpu/webgpu_cpp.h>

#include <Init.hpp>

class IMesh
{
    public:
        virtual size_t VertexCount() const = 0;
        virtual size_t IndexCount() const = 0;

        virtual bool IsUINT16() = 0;

        virtual void Clear() = 0;

        virtual void BuildMesh() = 0;
};

template<typename VertexT, typename IndexT>
class Mesh : public IMesh
{
    static_assert(std::is_same_v<IndexT, uint16_t> ||
                  std::is_same_v<IndexT, uint32_t>,
                  "Index must be uint16_t or uint32_t");
    
    
    public:
        using VertexType = VertexT;
        using IndexType = IndexT;

        std::vector<VertexT> vertices;
        std::vector<IndexT> indices;

        wgpu::Buffer vertexBuffer;
        wgpu::Buffer indexBuffer;
    
        Mesh() = default;

        Mesh(std::vector<VertexT> verts, std::vector<IndexT> inds)
        : vertices(std::move(verts)), indices(std::move(inds)) {}

        size_t VertexCount() const override { return vertices.size(); }
        size_t IndexCount()  const override { return indices.size();  }

        const VertexT& GetVertex(size_t i) const { return vertices[i]; }
        VertexT&       GetVertex(size_t i)       { return vertices[i]; }

        const IndexT& GetIndex(size_t i) const { return indices[i]; }
        IndexT&       GetIndex(size_t i)       { return indices[i]; }

        bool IsUINT16() override { return std::is_same_v<IndexT, uint16_t>; }

        void Clear() override {
            vertices.clear();
            indices.clear();
        }

        void BuildMesh() override
        {
            wgpu::BufferDescriptor bufferDesc;
            bufferDesc.size = vertices.size() * sizeof(VertexT);
            bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex; 
            bufferDesc.mappedAtCreation = false;
            vertexBuffer = device.CreateBuffer(&bufferDesc);

            device.GetQueue().WriteBuffer(vertexBuffer, 0, vertices.data(), bufferDesc.size);

            bufferDesc.size = indices.size() * sizeof(IndexT);
            bufferDesc.size = (bufferDesc.size + 3) & ~3;
            bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Index;
            indexBuffer = device.CreateBuffer(&bufferDesc);
            device.GetQueue().WriteBuffer(indexBuffer, 0, indices.data(), bufferDesc.size);
        }
};