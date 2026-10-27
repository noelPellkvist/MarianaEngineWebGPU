#pragma once
#include <Mariana/Scene/ECS.hpp>
#include <glm/glm.hpp>

// Transforms worked out from the LocalTRS chain: current as soon as something writes LocalTRS, before
// Scene::Update derives WorldXform (code that moves things and reads them back in the same frame:
// physics, scripts, IK targets).
glm::mat4 LocalMatrix(const LocalTRS& transform);
glm::mat4 WorldMatrix(const Scene& scene, Entity entity);
