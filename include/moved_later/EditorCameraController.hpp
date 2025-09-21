// EditorCameraController.hpp
#pragma once
#include "ICamera.hpp"              // <- include your interface
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

class EditorCameraController : public ICamera {
public:
    explicit EditorCameraController(IInput& input)
        : ICamera(input) {}

    // Call every frame (dt in seconds)
    void OnUpdate(float dt) override {
        // --- handle RMB lock/unlock like Unity ---
        if (_input.IsMouseButtonPressed(MouseButton::Right)) {
            _input.SetCursorLocked(true);
            _looking = true;
        }
        if (_input.IsMouseButtonReleased(MouseButton::Right)) {
            _input.SetCursorLocked(false);
            _looking = false;
        }

        // --- compute basis from yaw/pitch (RH, Y up) ---
        glm::vec3 forward{
            cosf(_pitch) * cosf(_yaw),
            sinf(_pitch),
            cosf(_pitch) * sinf(_yaw)
        };
        glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0,1,0), forward));
        glm::vec3 up    = glm::normalize(glm::cross(forward, right));

        // --- mouselook when RMB held ---
        if (_looking) {
            double dx, dy; _input.GetMouseDelta(dx, dy);
            _yaw   -= static_cast<float>(dx) * _mouseSens;
            _pitch -= static_cast<float>(dy) * _mouseSens;
            _pitch = std::clamp(_pitch, -_pitchLimit, _pitchLimit);
        }

        // --- WASD/QE fly when RMB held (Unity scene view) ---
        if (_looking) {
            float speed = _moveSpeed;

            glm::vec3 move(0);
            if (_input.IsKeyDown(Key::W)) move += forward;
            if (_input.IsKeyDown(Key::S)) move -= forward;
            if (_input.IsKeyDown(Key::D)) move += right;
            if (_input.IsKeyDown(Key::A)) move -= right;
            if (_input.IsKeyDown(Key::SPACE)) move += up;
            if (_input.IsKeyDown(Key::LEFT_SHIFT)) move -= up;

            if (glm::length(move) > 0.0f)
                _pos += glm::normalize(move) * speed * dt;
        }

        // --- MMB pan (only when not RMB looking, like Unity) ---
        if (_input.IsMouseButtonDown(MouseButton::Middle) && !_looking) {
            double dx, dy; _input.GetMouseDelta(dx, dy);
            // Pan scale grows with distance to avoid “snail pan” far away
            float dist = std::max(0.001f, glm::length(_pos - _pivot));
            _pos   -= right * static_cast<float>(dx) * _panPerPixel * dist;
            _pos   += up    * static_cast<float>(dy) * _panPerPixel * dist;
            _pivot -= right * static_cast<float>(dx) * _panPerPixel * dist;
            _pivot += up    * static_cast<float>(dy) * _panPerPixel * dist;
        }

        // --- Scroll wheel dolly (zoom) along forward ---
        {
            double sx, sy; _input.GetMouseScrollDelta(sx, sy);
            if (sy != 0.0) {
                // Exponential dolly speed proportional to scene scale
                float dist = std::max(0.001f, glm::length(_pos - _pivot));
                float step = static_cast<float>(sy) * _scrollDolly * std::max(1.0f, dist);
                _pos += forward * step;
            }
        }

        // Build view matrix (LH to match your original code)
        _view = glm::lookAtLH(_pos, _pos + forward, up);
    }

    // Configure / query (kept from your original)
    void SetPosition(const glm::vec3& p) { _pos = p; }
    void SetPivot(const glm::vec3& p)    { _pivot = p; }
    void SetYawPitch(float yaw, float pitch) {
        _yaw = yaw; _pitch = std::clamp(pitch, -_pitchLimit, _pitchLimit);
    }
    void SetMoveSpeed(float mps)           { _moveSpeed = mps; }
    void SetMouseSensitivity(float v)      { _mouseSens = v; }
    void SetPanPerPixel(float v)           { _panPerPixel = v; }
    void SetScrollDolly(float v)           { _scrollDolly = v; }

    float Yaw()   const { return _yaw; }
    float Pitch() const { return _pitch; }

private:
    // Using _input, _pos, _view from ICamera

    glm::vec3 _pivot{0, 0, 0};   // used for pan/scroll scaling
    float _yaw   = 0.0f;         // radians
    float _pitch = 0.0f;         // radians
    float _pitchLimit = glm::radians(89.0f);

    // Tunables (feel free to tweak to taste)
    float _moveSpeed   = 6.0f;    // m/s
    float _sprintMult  = 4.0f;
    float _slowMult    = 0.25f;
    float _mouseSens   = 0.0025f; // rad per pixel
    float _panPerPixel = 0.0015f; // world-units per pixel (scaled by dist)
    float _scrollDolly = 0.08f;   // world-units per scroll notch (scaled by dist)

    bool _looking = false;
};
