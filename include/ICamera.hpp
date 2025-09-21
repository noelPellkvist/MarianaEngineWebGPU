#pragma once
#include <glm/glm.hpp>
#include <moved_later/IInput.hpp>

class ICamera
{
    public:
        ICamera(IInput& input) : _input(input) {}
        virtual ~ICamera() = default;

        virtual void OnUpdate(float deltaTime) {}

        const glm::mat4& View() const { return _view; }
        glm::vec3 Position()   const { return _pos; }

    protected:
        IInput& _input;
        glm::vec3 _pos;
        glm::mat4 _view{1.0f};
};