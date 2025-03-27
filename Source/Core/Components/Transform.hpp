#pragma once

#include <vector>
#include <glm.hpp>
#include <gtc/quaternion.hpp>
#include <entt/entt.hpp>
#include <string>

struct TransformBufferData
{
    glm::mat4 modelMatrix{1.0f};
    glm::mat4 normalMatrix{1.0f};
};

struct Transform
{
    public:
        Transform(std::string name) : name(name) { }
        glm::vec3 position;
        glm::quat rotation;
        glm::vec3 scale;
        glm::vec3 eulerAngles;
        int dataIndex;
        std::string name;
        TransformBufferData data;
        


    void SetPosition(glm::vec3 newPosition);
};



struct Dirty
{   
    
};
