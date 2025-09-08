#pragma once
#include <vector>
#include <webgpu/webgpu_cpp.h>

#include <Init.hpp>

template<typename VertexT, typename IndexT>
class Mesh
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
    
        Mesh() = default;

        Mesh(std::vector<VertexT> verts, std::vector<IndexT> inds)
        : vertices(std::move(verts)), indices(std::move(inds)) {}

        size_t VertexCount() const { return vertices.size(); }
        size_t IndexCount()  const { return indices.size();  }

        const VertexT& GetVertex(size_t i) const { return vertices[i]; }
        VertexT&       GetVertex(size_t i)       { return vertices[i]; }

        const IndexT& GetIndex(size_t i) const { return indices[i]; }
        IndexT&       GetIndex(size_t i)       { return indices[i]; }

        void Clear() {
            vertices.clear();
            indices.clear();
        }

        void BuildMesh()
        {
            wgpu::BufferDescriptor bufferDesc;
            bufferDesc.size = vertices.size() * sizeof(VertexT);
            bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex; 
            bufferDesc.mappedAtCreation = false;
            vertexBuffer = device.CreateBuffer(&bufferDesc);

            device.GetQueue().WriteBuffer(vertexBuffer, 0, vertices.data(), bufferDesc.size);
        }
};