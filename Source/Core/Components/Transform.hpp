#pragma once

#include <vector>
#include <glm.hpp>
#include <gtc/quaternion.hpp>
#include <entt/entt.hpp>

struct Transform
{
    public:
        Transform() { }
        glm::vec3 position;
        glm::quat rotation;
        glm::vec3 scale;
        glm::vec3 eulerAngles;
        int dataIndex;


    void SetPosition(glm::vec3 newPosition);
};

struct TransformData
{
    glm::mat4 modelMatrix;
    glm::mat4 normalMatrix;
};
