#include "Transform.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include <gtx/quaternion.hpp>

void Transform::SetPosition(glm::vec3 newPosition)
{
    position = newPosition;
    RecalculateMatrix();
}

void Transform::RecalculateMatrix()
{
    modelMatrix = glm::translate(glm::mat4(1.0f), position) *
                  glm::toMat4(rotation);
    modelMatrix = glm::scale(modelMatrix, scale);
}