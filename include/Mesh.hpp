#pragma once
#include <vector>
#include <memory>

struct Submesh
{
    uint32_t startIndex{0};
    uint32_t indexCount{0};
    uint32_t materialIndex{0};
};


class IMesh
{
    public:
        IMesh();
        virtual ~IMesh(); 
        virtual size_t VertexCount() const = 0;
        virtual size_t IndexCount() const = 0;

        virtual bool IsUINT16() = 0;

        virtual void Clear() = 0;

        virtual void BuildMesh() = 0;

        void* GetVertexBuffer();
        void* GetIndexBuffer();

        struct Impl;
        std::unique_ptr<Impl> impl;
        std::vector<Submesh> submeshes;



    protected:
        void _buildMesh(void* vertices, size_t verticesLength, size_t vertexSize, void* indices, size_t indicesLength, size_t indexSize);
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
    
        Mesh() {}

        Mesh(std::vector<VertexT> verts, std::vector<IndexT> inds)
        : vertices(std::move(verts)), indices(std::move(inds)) {
            
        }

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
            _buildMesh(vertices.data(), vertices.size(), sizeof(VertexT), indices.data(), indices.size(), sizeof(IndexT));
        }
};