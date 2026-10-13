#pragma once
#include <cstdint>
#include <memory>
#include <vector>

#include <Mesh.hpp>
#include <Vertex.hpp>

// How a section's indices are meant (the pipelines only draw the list forms; the rest is converted).
enum class SourceTopology { Points, Lines, LineLoop, LineStrip, Triangles, TriangleStrip, TriangleFan };

// One part of a mesh with its own vertices: a glTF primitive, or anything built in code. It becomes
// one submesh, drawn with the material at `materialIndex` of the entity's MeshComponent.
struct MeshSection
{
    std::vector<StandardVertex> vertices;
    std::vector<uint32_t> indices; // empty: one per vertex, in order
    SourceTopology topology = SourceTopology::Triangles;
    uint32_t materialIndex = 0;
    // Without normals, triangles get flat (per-face) normals and any tangents are ignored.
    bool hasNormals = true;
    // Without tangents, they are generated from the normals and texcoord0 (for normal maps).
    bool hasTangents = false;
};

// Builds a GPU mesh from sections: converts strips, fans and loops to lists, generates flat
// normals and tangents where missing, uses 16-bit indices when possible, and computes bounds.
// Returns nullptr when there is nothing to draw.
std::shared_ptr<IMesh> BuildMesh(std::vector<MeshSection> sections);

// The steps BuildMesh uses, for code that needs them on their own.
namespace Geometry
{
    // Converts strips, fans and loops to plain lists (keeping every triangle's winding) and returns
    // what is left to draw. Incomplete trailing primitives are dropped.
    PrimitiveTopology ToListTopology(SourceTopology topology, std::vector<uint32_t>& indices);
    // Gives every triangle its own three vertices with the face normal; indices become 0, 1, 2, ...
    void GenerateFlatNormals(std::vector<StandardVertex>& vertices, std::vector<uint32_t>& indices);
    // Per-vertex tangents from the triangles' texcoord0 directions (Lengyel's method), with the
    // handedness glTF expects (the bitangent points up the texture).
    void GenerateTangents(std::vector<StandardVertex>& vertices, const std::vector<uint32_t>& indices);
}
