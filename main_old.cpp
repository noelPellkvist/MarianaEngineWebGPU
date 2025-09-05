#include <iostream>
#include <glm/glm.hpp>

#include "BufferReflection.hpp"

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec4 color;
    glm::vec2 texCoord;
    REGISTER_BUFFER_ENTRIES(
        BUFFER_ENTRY_INFO(Vertex, position),
        BUFFER_ENTRY_INFO(Vertex, normal),
        BUFFER_ENTRY_INFO(Vertex, color),
        BUFFER_ENTRY_INFO(Vertex, texCoord)
    )
};

int main() {
    Vertex s{
        glm::vec3(1.0f, 2.0f, 3.0f),      // position
        glm::vec3(4.0f, 5.0f, 6.0f),      // normal
        glm::vec4(0.1f, 0.2f, 0.3f, 0.4f),// color
        glm::vec2(7.0f, 8.0f)             // texCoord
    };
    RegisterVertexBuffer(s, Vertex::get_entries());
    return 0;
}