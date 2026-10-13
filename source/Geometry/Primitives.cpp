#include <Geometry/Primitives.hpp>

#include <algorithm>
#include <cmath>
#include <glm/gtc/constants.hpp>

namespace
{
    StandardVertex MakeVertex(const glm::vec3& position, const glm::vec3& normal, const glm::vec2& uv)
    {
        StandardVertex vertex{};
        vertex.position = position;
        vertex.normal = normal;
        vertex.texcoord0 = uv;
        vertex.color0 = glm::vec4(1.0f);
        return vertex;
    }

    // Two triangles a b c, a c d whose front faces (cross(p1 - p0, p2 - p0)) point along `normal`.
    // The winding is judged from the diagonals, which stay valid when two corners meet (sphere poles).
    void AddQuad(MeshSection& section, uint32_t a, uint32_t b, uint32_t c, uint32_t d, const glm::vec3& normal)
    {
        const std::vector<StandardVertex>& v = section.vertices;
        const glm::vec3 face = glm::cross(v[c].position - v[a].position, v[d].position - v[b].position);
        if (glm::dot(face, normal) >= 0.0f)
            section.indices.insert(section.indices.end(), { a, b, c, a, c, d });
        else
            section.indices.insert(section.indices.end(), { a, c, b, a, d, c });
    }
}

namespace Primitives
{
    MeshSection Cube(float size)
    {
        MeshSection section;
        const float h = size * 0.5f;
        const glm::vec3 axes[3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
        for (int axis = 0; axis < 3; ++axis)
        {
            for (float side : { -1.0f, 1.0f })
            {
                const glm::vec3 n = axes[axis] * side;
                const glm::vec3 u = axes[(axis + 1) % 3];
                const glm::vec3 v = axes[(axis + 2) % 3];
                const uint32_t base = static_cast<uint32_t>(section.vertices.size());
                for (glm::vec2 corner : { glm::vec2(-1, -1), glm::vec2(1, -1), glm::vec2(1, 1), glm::vec2(-1, 1) })
                {
                    const glm::vec3 p = (n + u * corner.x + v * corner.y) * h;
                    section.vertices.push_back(MakeVertex(p, n, corner * 0.5f + 0.5f));
                }
                AddQuad(section, base, base + 1, base + 2, base + 3, n);
            }
        }
        return section;
    }

    MeshSection Plane(float size, uint32_t subdivisions)
    {
        MeshSection section;
        const uint32_t cells = std::max(subdivisions, 1u);
        for (uint32_t z = 0; z <= cells; ++z)
        {
            for (uint32_t x = 0; x <= cells; ++x)
            {
                const glm::vec2 t(static_cast<float>(x) / cells, static_cast<float>(z) / cells);
                section.vertices.push_back(MakeVertex(glm::vec3((t.x - 0.5f) * size, 0.0f, (t.y - 0.5f) * size), glm::vec3(0, 1, 0), t));
            }
        }
        for (uint32_t z = 0; z < cells; ++z)
        {
            for (uint32_t x = 0; x < cells; ++x)
            {
                const uint32_t a = z * (cells + 1) + x;
                AddQuad(section, a, a + 1, a + cells + 2, a + cells + 1, glm::vec3(0, 1, 0));
            }
        }
        return section;
    }

    MeshSection Sphere(float radius, uint32_t segments, uint32_t rings)
    {
        MeshSection section;
        segments = std::max(segments, 3u);
        rings = std::max(rings, 2u);
        for (uint32_t r = 0; r <= rings; ++r)
        {
            const float polar = glm::pi<float>() * r / rings;
            for (uint32_t s = 0; s <= segments; ++s)
            {
                const float azimuth = glm::two_pi<float>() * s / segments;
                const glm::vec3 n(std::sin(polar) * std::cos(azimuth), std::cos(polar), std::sin(polar) * std::sin(azimuth));
                section.vertices.push_back(MakeVertex(n * radius, n, glm::vec2(static_cast<float>(s) / segments, static_cast<float>(r) / rings)));
            }
        }
        for (uint32_t r = 0; r < rings; ++r)
        {
            for (uint32_t s = 0; s < segments; ++s)
            {
                const uint32_t a = r * (segments + 1) + s;
                const uint32_t b = a + segments + 1;
                // Outward is the direction from the centre through the quad.
                const glm::vec3 centre = section.vertices[a].normal + section.vertices[b + 1].normal;
                AddQuad(section, a, a + 1, b + 1, b, centre);
            }
        }
        return section;
    }
}
