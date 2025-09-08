#pragma once
#include <Shader.hpp>
#include <Buffers.hpp>

class Material {
    public:
        Material(Shader& shader);
        ~Material();

    private:
        Shader& m_Shader;
        Buffers m_Buffers;
};