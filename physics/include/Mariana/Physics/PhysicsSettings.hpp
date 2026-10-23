#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <glm/glm.hpp>

// How a scene's simulation runs, and its collision layers: named groups and which of them collide
// (static bodies never collide with each other). Kept in the project's /Physics.json:
//   {
//     "gravity": [0, -9.81, 0], "fixedTimestep": 0.016667, "maxStepsPerFrame": 4, "collisionSteps": 1,
//     "interpolation": true, "penetrationSlop": 0.005, "speculativeContactDistance": 0.02,
//     "layers": [ { "name": "Default", "collidesWith": [ "Default", "Debris" ] }, { "name": "Debris", ... } ]
//   }
// Missing values keep the defaults.
struct PhysicsSettings
{
    static constexpr int kMaxLayers = 32;

    glm::vec3 gravity{ 0.0f, -9.81f, 0.0f };
    // The simulation runs in steps of this many seconds, however long frames are.
    float fixedTimestep = 1.0f / 60.0f;
    // At most this many steps a frame; past that the simulation runs slower than game time.
    int maxStepsPerFrame = 4;
    // Collision detection passes per step (more for fast or heavily stacked scenes).
    int collisionSteps = 1;
    // Moving bodies are shown between where their last two steps left them (smooth however the
    // frames fall between steps, one step behind); off: where the last step left them.
    bool interpolation = true;
    // How far bodies may sink into each other before they are pushed apart (m), and how close they
    // must come before a contact starts being solved.
    float penetrationSlop = 0.005f;
    float speculativeContactDistance = 0.02f;

    std::vector<std::string> layers{ "Default" };
    std::vector<uint32_t> collisions{ ~0u }; // per layer, a bit per layer it collides with

    bool Collides(int a, int b) const;
    // Both ways.
    void SetCollides(int a, int b, bool collides);
    // The new layer's index (it collides with every layer), or -1 when there are kMaxLayers.
    int AddLayer(std::string name);

    bool Load(const std::filesystem::path& file, std::string& error);
    bool Save(const std::filesystem::path& file, std::string& error) const;
};
