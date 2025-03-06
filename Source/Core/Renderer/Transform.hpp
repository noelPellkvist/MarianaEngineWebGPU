#pragma once

#include <vector>
#include <glm.hpp>
#include <gtc/quaternion.hpp>
#include <entt/entt.hpp>

struct Transform
{
    private:
        glm::mat4 modelMatrix;
        entt::entity parent = entt::null;
        std::vector<entt::entity> children;
        
    public:
        Transform(entt::entity parentTransform = entt::null) : parent(parentTransform) {}
        glm::vec3 position;
        glm::quat rotation;
        glm::vec3 scale;
        glm::vec3 eulerAngles;
        bool dirty = false;

        void RecalculateMatrix();
        const glm::mat4& getLocalMatrix() { return modelMatrix; }


    void SetPosition(glm::vec3 newPosition);
};
