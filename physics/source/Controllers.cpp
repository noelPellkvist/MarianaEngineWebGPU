#include <Mariana/Physics/Controllers.hpp>

#include "WorldImpl.hpp"

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <functional>

using namespace PhysicsInternal;

void ChaseCamera::Update(const glm::mat4& target, const FollowCamera& settings, float dt)
{
    const glm::vec3 up(0.0f, 1.0f, 0.0f);
    const glm::vec3 base(target[3]);
    // Behind the target along its forward, kept level so the camera doesn't pitch with the body.
    glm::vec3 forward(target[2]);
    forward.y = 0.0f;
    forward = glm::length(forward) > 1e-4f ? glm::normalize(forward) : glm::vec3(0.0f, 0.0f, 1.0f);
    const glm::vec3 wantedPosition = base - forward * settings.distance + up * settings.height;
    const glm::vec3 wantedLook = base + up * settings.lookHeight;

    // Far off (the first time, or after a jump): be there at once.
    if (!placed || glm::length(position - wantedPosition) > 4.0f * std::max(settings.distance, 1.0f))
    {
        position = wantedPosition;
        lookAt = wantedLook;
        placed = true;
        return;
    }
    const float k = 1.0f - std::exp(-std::max(settings.stiffness, 0.0f) * dt);
    position += (wantedPosition - position) * k;
    // The look point follows faster, so the target stays near the middle of the view.
    const float kLook = 1.0f - std::exp(-std::max(settings.stiffness, 0.0f) * 3.0f * dt);
    lookAt += (wantedLook - lookAt) * kLook;
}

void UpdateFollowCameras(Scene& scene, float dt)
{
    // Targets by name, and the first entity with a PlayerControl.
    Entity firstControlled;
    std::vector<std::pair<std::string, Entity>> named;
    std::vector<Entity> cameras;
    std::function<void(Entity)> visit = [&](Entity e) {
        if (!firstControlled.IsValid() && e.TryGet<PlayerControl>())
            firstControlled = e;
        if (e.TryGet<FollowCamera>())
            cameras.push_back(e);
        named.emplace_back(e.GetName(), e);
        e.ForEachChild(visit);
    };
    scene.ForEachRoot([&](Entity root) { visit(root); });

    for (Entity camera : cameras)
    {
        const FollowCamera& settings = *camera.TryGet<FollowCamera>();
        Entity target = firstControlled;
        if (!settings.target.empty())
        {
            target = {};
            for (const auto& [name, e] : named)
                if (name == settings.target)
                {
                    target = e;
                    break;
                }
        }
        LocalTRS* trs = camera.Get<LocalTRS>();
        if (!target.IsValid() || !trs || target.RawId() == camera.RawId())
            continue;

        // The camera's current place is where the chase goes on from.
        const glm::mat4 cameraWorld = WorldMatrix(scene, camera);
        ChaseCamera chase;
        chase.position = glm::vec3(cameraWorld[3]);
        chase.lookAt = chase.position + glm::normalize(glm::vec3(cameraWorld[2])) * std::max(settings.distance, 1.0f);
        chase.placed = true;
        chase.Update(WorldMatrix(scene, target), settings, dt);
        const glm::vec3 direction = chase.lookAt - chase.position;
        if (glm::length(direction) < 1e-4f)
            continue;
        // Cameras look along their +Z with +Y up.
        const glm::quat rotation = glm::quatLookAtLH(glm::normalize(direction), glm::vec3(0.0f, 1.0f, 0.0f));
        SetWorldPose(scene, camera, *trs, chase.position, rotation);
    }
}
