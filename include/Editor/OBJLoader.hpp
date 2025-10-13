#pragma once

#include <string>

#include <Mesh.hpp>
#include <glm/glm.hpp>

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

Mesh<Vertex, uint16_t> LoadTestMesh();

Mesh<Vertex, uint32_t> LoadOBJMesh(const std::string &filename);