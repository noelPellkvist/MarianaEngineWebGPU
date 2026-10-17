#include <Mariana/Rendering/SceneCamera.hpp>

#include <algorithm>
#include <glm/gtc/type_ptr.hpp>

void SceneCamera::Follow(const WorldXform& world, const CameraComponent& camera)
{
    const glm::mat4 model = glm::make_mat4(world.model);
    const glm::vec3 forward = glm::normalize(glm::vec3(model[2]));
    const glm::vec3 up = glm::normalize(glm::vec3(model[1]));
    _pos = glm::vec3(model[3]);
    _view = glm::lookAtLH(_pos, _pos + forward, up);
    fov = camera.fov;
    nearClip = camera.nearClip;
    farClip = std::max(camera.farClip, camera.nearClip * 2.0f);
}
