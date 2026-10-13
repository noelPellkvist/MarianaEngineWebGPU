#pragma once
#include <vector>
#include <memory>
#include <cfloat>
#include <glm/glm.hpp>

// glTF strips, fans and loops are converted to these lists when loaded.
enum class PrimitiveTopology : uint8_t { Triangles, Lines, Points };

struct Submesh
{
    uint32_t startIndex{0};
    uint32_t indexCount{0};
    uint32_t materialIndex{0};
    PrimitiveTopology topology = PrimitiveTopology::Triangles;
};

// An axis-aligned box, empty (min > max) until something is added to it.
struct Bounds
{
    glm::vec3 min{ FLT_MAX };
    glm::vec3 max{ -FLT_MAX };

    bool IsEmpty() const { return min.x > max.x; }
    void Add(const glm::vec3& p) { min = glm::min(min, p); max = glm::max(max, p); }
};


class IMesh
{
    public:
        IMesh();
        virtual ~IMesh(); 
        // Counts of what was uploaded by BuildMesh (the CPU arrays may be cleared afterwards).
        size_t VertexCount() const { return m_VertexCount; }
        size_t IndexCount() const { return m_IndexCount; }

        virtual bool IsUINT16() = 0;

        virtual void Clear() = 0;

        virtual void BuildMesh() = 0;

        void* GetVertexBuffer();
        void* GetIndexBuffer();

        struct Impl;
        std::unique_ptr<Impl> impl;
        std::vector<Submesh> submeshes;

        // Object-space bounds of the vertex positions, filled by BuildMesh (used for culling).
        glm::vec3 boundsMin{0.0f};
        glm::vec3 boundsMax{0.0f};
        bool hasBounds = false;

        // Skinned meshes: bind-pose bounds of the vertices each joint influences, indexed like the
        // vertices' boneIndices (empty when nothing is weighted). Moved by its joint, each box still
        // contains those vertices, so together they bound the posed mesh.
        std::vector<Bounds> jointBounds;

    protected:
        size_t m_VertexCount = 0;
        size_t m_IndexCount = 0;

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

        const VertexT& GetVertex(size_t i) const { return vertices[i]; }
        VertexT&       GetVertex(size_t i)       { return vertices[i]; }

        const IndexT& GetIndex(size_t i) const { return indices[i]; }
        IndexT&       GetIndex(size_t i)       { return indices[i]; }

        bool IsUINT16() override { return std::is_same_v<IndexT, uint16_t>; }

        // Frees the CPU-side arrays (clear() alone would keep their memory).
        void Clear() override {
            std::vector<VertexT>().swap(vertices);
            std::vector<IndexT>().swap(indices);
        }

        void BuildMesh() override
        {
            if constexpr (requires(const VertexT& v) { glm::vec3(v.position); })
            {
                hasBounds = !vertices.empty();
                if (hasBounds)
                {
                    boundsMin = boundsMax = glm::vec3(vertices[0].position);
                    for (const VertexT& v : vertices)
                    {
                        boundsMin = glm::min(boundsMin, glm::vec3(v.position));
                        boundsMax = glm::max(boundsMax, glm::vec3(v.position));
                    }
                }
            }
            if constexpr (requires(const VertexT& v) { glm::vec3(v.position); glm::uvec4(v.boneIndices); glm::vec4(v.boneWeights); })
            {
                jointBounds.clear();
                for (const VertexT& v : vertices)
                {
                    for (int k = 0; k < 4; ++k)
                    {
                        if (v.boneWeights[k] <= 0.0f)
                            continue;
                        const uint32_t joint = v.boneIndices[k];
                        if (joint >= jointBounds.size())
                            jointBounds.resize(joint + 1);
                        jointBounds[joint].Add(glm::vec3(v.position));
                    }
                }
            }
            _buildMesh(vertices.data(), vertices.size(), sizeof(VertexT), indices.data(), indices.size(), sizeof(IndexT));
        }
};