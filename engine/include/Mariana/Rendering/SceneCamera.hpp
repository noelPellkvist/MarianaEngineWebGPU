#pragma once
#include <Mariana/Rendering/ICamera.hpp>
#include <Mariana/Rendering/Components.hpp>
#include <Mariana/Scene/ECS.hpp>

// A camera entity seen through ICamera: the view follows its world transform (looking along its +z
// axis, +y up) and the projection its CameraComponent. Call Follow each frame, then UpdateBuffer.
class SceneCamera : public ICamera
{
public:
    explicit SceneCamera(IInput& input) : ICamera(input) {}

    void Follow(const WorldXform& world, const CameraComponent& camera);
};
