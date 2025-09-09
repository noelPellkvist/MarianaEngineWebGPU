#include <moved_later/OBJLoader.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include <moved_later/tiny_obj_loader.h>


Mesh<Vertex, uint16_t> LoadTestMesh()
{
    std::vector<Vertex> verts = {
  {{-0.5,-0.5,0}, {1,0,0}},
  {{0.5,-0.5,0}, {0,1,0}},
  {{0.5,0.5,0}, {0,0,1}},
  {{-0.5,0.5,0}, {1,1,0}}
};

std::vector<uint16_t> indices = {
  0, 1, 2,
  0, 2, 3
};

return Mesh<Vertex, uint16_t>(verts, indices);
}

Mesh<Vertex, uint32_t> LoadOBJMesh(const std::string &filename)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename.c_str())) {
        throw std::runtime_error(err);
    }

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            vertex.position = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            vertex.normal = {
                attrib.normals[3 * index.normal_index + 0],
                attrib.normals[3 * index.normal_index + 1],
                attrib.normals[3 * index.normal_index + 2]
            };
        
            vertices.push_back(vertex);
            indices.push_back(indices.size());
        }
    }

    return Mesh<Vertex, uint32_t>(vertices, indices);
}