#include <Mariana/Geometry/MeshBuilder.hpp>

#include <cmath>
#include <utility>

namespace Geometry
{
    PrimitiveTopology ToListTopology(SourceTopology topology, std::vector<uint32_t>& indices)
    {
        std::vector<uint32_t> list;
        switch (topology)
        {
            case SourceTopology::Points:
                return PrimitiveTopology::Points;
            case SourceTopology::Lines:
                indices.resize(indices.size() & ~size_t(1));
                return PrimitiveTopology::Lines;
            case SourceTopology::LineLoop:
            case SourceTopology::LineStrip:
                for (size_t i = 0; i + 1 < indices.size(); ++i) {
                    list.push_back(indices[i]);
                    list.push_back(indices[i + 1]);
                }
                if (topology == SourceTopology::LineLoop && indices.size() > 2) {
                    list.push_back(indices.back());
                    list.push_back(indices.front());
                }
                indices.swap(list);
                return PrimitiveTopology::Lines;
            case SourceTopology::TriangleStrip:
                // Every other triangle of a strip is wound the other way, so it is flipped back.
                for (size_t i = 0; i + 2 < indices.size(); ++i) {
                    const bool odd = (i % 2) == 1;
                    list.push_back(indices[i]);
                    list.push_back(indices[i + (odd ? 2 : 1)]);
                    list.push_back(indices[i + (odd ? 1 : 2)]);
                }
                indices.swap(list);
                return PrimitiveTopology::Triangles;
            case SourceTopology::TriangleFan:
                for (size_t i = 0; i + 2 < indices.size(); ++i) {
                    list.push_back(indices[i + 1]);
                    list.push_back(indices[i + 2]);
                    list.push_back(indices[0]);
                }
                indices.swap(list);
                return PrimitiveTopology::Triangles;
            case SourceTopology::Triangles:
            default:
                indices.resize(indices.size() - indices.size() % 3);
                return PrimitiveTopology::Triangles;
        }
    }

    void GenerateFlatNormals(std::vector<StandardVertex>& vertices, std::vector<uint32_t>& indices)
    {
        std::vector<StandardVertex> flat;
        flat.reserve(indices.size());
        for (size_t i = 0; i + 2 < indices.size(); i += 3)
        {
            if (indices[i] >= vertices.size() || indices[i + 1] >= vertices.size() || indices[i + 2] >= vertices.size())
                continue;
            StandardVertex a = vertices[indices[i]];
            StandardVertex b = vertices[indices[i + 1]];
            StandardVertex c = vertices[indices[i + 2]];
            // Front faces are the side cross(p1 - p0, p2 - p0) points to.
            const glm::vec3 n = glm::cross(b.position - a.position, c.position - a.position);
            a.normal = b.normal = c.normal = glm::dot(n, n) > 1e-20f ? glm::normalize(n) : glm::vec3(0.0f, 0.0f, 1.0f);
            flat.push_back(a);
            flat.push_back(b);
            flat.push_back(c);
        }
        vertices.swap(flat);
        indices.resize(vertices.size());
        for (size_t i = 0; i < indices.size(); ++i)
            indices[i] = static_cast<uint32_t>(i);
    }

    void GenerateTangents(std::vector<StandardVertex>& vertices, const std::vector<uint32_t>& indices)
    {
        // Each triangle's direction of increasing u (and v) is summed on its vertices, then made
        // perpendicular to the normal. w picks the bitangent side so it points up the texture
        // (decreasing v), as glTF expects.
        std::vector<glm::vec3> tangents(vertices.size(), glm::vec3(0.0f));
        std::vector<glm::vec3> bitangents(vertices.size(), glm::vec3(0.0f));
        for (size_t i = 0; i + 2 < indices.size(); i += 3)
        {
            const uint32_t i0 = indices[i], i1 = indices[i + 1], i2 = indices[i + 2];
            if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size())
                continue;

            const glm::vec3 e1 = vertices[i1].position - vertices[i0].position;
            const glm::vec3 e2 = vertices[i2].position - vertices[i0].position;
            const glm::vec2 d1 = vertices[i1].texcoord0 - vertices[i0].texcoord0;
            const glm::vec2 d2 = vertices[i2].texcoord0 - vertices[i0].texcoord0;
            const float det = d1.x * d2.y - d2.x * d1.y;
            if (std::abs(det) < 1e-12f)
                continue;

            const glm::vec3 t = (e1 * d2.y - e2 * d1.y) / det;
            const glm::vec3 b = (e2 * d1.x - e1 * d2.x) / det;
            for (const uint32_t index : { i0, i1, i2 })
            {
                tangents[index] += t;
                bitangents[index] += b;
            }
        }

        for (size_t i = 0; i < vertices.size(); ++i)
        {
            const glm::vec3 n = vertices[i].normal;
            glm::vec3 t = tangents[i] - n * glm::dot(n, tangents[i]);
            // No usable UVs around this vertex: any direction along the surface.
            if (glm::dot(t, t) < 1e-12f)
                t = glm::cross(n, std::abs(n.y) < 0.99f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f));
            t = glm::dot(t, t) > 1e-20f ? glm::normalize(t) : glm::vec3(1.0f, 0.0f, 0.0f);
            const float w = glm::dot(glm::cross(n, t), bitangents[i]) < 0.0f ? 1.0f : -1.0f;
            vertices[i].tangent = glm::vec4(t, w);
        }
    }
}

std::shared_ptr<IMesh> BuildMesh(std::vector<MeshSection> sections)
{
    std::vector<StandardVertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Submesh> submeshes;

    for (MeshSection& section : sections)
    {
        if (section.vertices.empty())
            continue;
        if (section.indices.empty())
        {
            section.indices.resize(section.vertices.size());
            for (size_t i = 0; i < section.indices.size(); ++i)
                section.indices[i] = static_cast<uint32_t>(i);
        }

        const PrimitiveTopology topology = Geometry::ToListTopology(section.topology, section.indices);
        if (topology == PrimitiveTopology::Triangles)
        {
            if (!section.hasNormals)
                Geometry::GenerateFlatNormals(section.vertices, section.indices);
            if (!section.hasNormals || !section.hasTangents)
                Geometry::GenerateTangents(section.vertices, section.indices);
        }
        if (section.indices.empty())
            continue;

        const uint32_t baseVertex = static_cast<uint32_t>(vertices.size());
        Submesh submesh{};
        submesh.startIndex = static_cast<uint32_t>(indices.size());
        submesh.indexCount = static_cast<uint32_t>(section.indices.size());
        submesh.materialIndex = section.materialIndex;
        submesh.topology = topology;
        submeshes.push_back(submesh);

        vertices.insert(vertices.end(), section.vertices.begin(), section.vertices.end());
        indices.reserve(indices.size() + section.indices.size());
        for (uint32_t i : section.indices)
            indices.push_back(i + baseVertex);
    }

    if (vertices.empty() || indices.empty())
        return nullptr;

    std::shared_ptr<IMesh> mesh;
    if (vertices.size() <= 65536)
    {
        // Every index fits in 16 bits: half the index memory and bandwidth.
        std::vector<uint16_t> shortIndices(indices.begin(), indices.end());
        mesh = std::make_shared<Mesh<StandardVertex, uint16_t>>(std::move(vertices), std::move(shortIndices));
    }
    else
    {
        mesh = std::make_shared<Mesh<StandardVertex, uint32_t>>(std::move(vertices), std::move(indices));
    }
    mesh->submeshes = std::move(submeshes);
    mesh->BuildMesh();
    // The GPU has its own copy now; only the counts and bounds are kept on the CPU.
    mesh->Clear();
    return mesh;
}
