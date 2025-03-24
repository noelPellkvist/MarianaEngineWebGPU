#include "Transform.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include <gtx/quaternion.hpp>

void Transform::SetPosition(glm::vec3 newPosition)
{
    position = newPosition;
}