#pragma once

#include "Renderer/Mesh.hpp"
#include "Renderer/Shader.hpp"
#include <string>

class Resources 
{
    public:
        static std::string LoadString(const std::string& path);
        static Mesh LoadObjMesh(const std::string& path, const Shader& shader);
};