#pragma once

#include <string>

#include <Mesh.hpp>
#include <Init.hpp>

Mesh<Vertex, uint16_t> LoadTestMesh();

Mesh<Vertex, uint32_t> LoadOBJMesh(const std::string &filename);