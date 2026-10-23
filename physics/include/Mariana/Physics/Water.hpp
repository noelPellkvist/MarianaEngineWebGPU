#pragma once
#include <glm/glm.hpp>

// A body of water: the box `size` (scaled and turned with its entity) centred on the entity, its
// top face the surface. Dynamic bodies in it are held up by the water they push aside (Jolt's
// buoyancy, so bodies lighter than water float and heavier ones sink; RigidBody::buoyancy scales
// it), slowed by `linearDrag` and `angularDrag` and carried along by `flow`. Waves move the surface
// they float on, and while the game runs the normal maps of the meshes on the entity and below it
// scroll by `rippleSpeed`. Characters wade in it and swim: they weigh less as they go under, move
// slower, and a jump swims up. A trigger collider on the entity reports what goes in and out.
struct WaterVolume
{
    glm::vec3 size{ 10.0f, 2.0f, 10.0f };
    float density = 1000.0f;   // kg/m^3 (fresh water)
    float linearDrag = 0.5f;   // Jolt's drag factors (quadratic in speed)
    float angularDrag = 0.05f;
    float damping = 1.0f;      // 1/s, times how far under: calms what floats (slow bobbing, spinning)
    glm::vec3 flow{ 0.0f };    // m/s, world space
    float waveHeight = 0.0f;   // m the crests rise above the still surface
    float waveLength = 6.0f;   // m
    float waveSpeed = 1.5f;    // m/s
    glm::vec2 rippleSpeed{ 0.04f, 0.025f }; // texture coordinates per second
};

// Where the water's surface is above or below a world point (its height there and its normal),
// `time` seconds into the waves; `top` is the surface's still center, `up`, `right` and `forward`
// its axes.
struct WaterSurface
{
    glm::vec3 point{ 0.0f };   // on the surface, straight above or below the point asked about
    glm::vec3 normal{ 0.0f, 1.0f, 0.0f };
};
WaterSurface WaterSurfaceAt(const WaterVolume& water, const glm::vec3& top, const glm::vec3& up, const glm::vec3& right, const glm::vec3& forward,
                            const glm::vec3& point, float time);
