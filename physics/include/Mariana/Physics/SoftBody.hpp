#pragma once
#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>

// Which of a soft body's vertices hold on to its entity (they move with it, not with the
// simulation): those along a side of its mesh's bounds in the entity's own axes (Top = +Y, Right =
// +X, Front = +Z), the four corners of its two widest axes, or the two top ones of those.
enum class SoftBodyPin : uint8_t { None, Top, Bottom, Left, Right, Front, Back, Corners, TopCorners };

// A soft body: goes on an entity with a MeshComponent, whose mesh it deforms. Made when the
// simulation starts from the mesh's triangles: vertices at the same place become one particle
// (sharing `mass`), triangles its faces, their edges springs (with shear springs across quads,
// bend springs across edges when `bendStiffness` > 0 and, when something is pinned, long range
// attachments that keep each particle within its rest distance of the nearest pin, so cloth hangs
// without stretching). The entity draws a copy of its mesh that is written every frame (the mesh
// it had comes back when the soft body goes); a mesh made without its data kept on the CPU
// (MeshUsage::KeepData, as the builtin primitives are) is copied from its collision triangles,
// without texture coordinates. Stiffness is relative to the particles' masses and pressure to its
// weight, so they mean the same for any mass, size or resolution. A closed mesh with `pressure` is
// pumped up like a ball: its gas pushes its surface out, the harder the more it is squashed.
struct SoftBody
{
    float mass = 1.0f;            // kg, shared by the particles
    float stiffness = 0.9f;       // edges: 0 = rubber band .. 1 = doesn't stretch
    float bendStiffness = 0.0f;   // 0 = folds freely .. 1 = keeps its shape
    float pressure = 0.0f;        // closed meshes: the gas's push at rest size in its weights (10 = soft ball, 50 = firm)
    float friction = 0.5f;
    float restitution = 0.0f;
    float damping = 0.1f;         // 1/s
    float gravityFactor = 1.0f;
    uint32_t iterations = 5;      // solver sub-steps per physics step
    float thickness = 0.02f;      // m it keeps off what it touches
    SoftBodyPin pin = SoftBodyPin::None;
    glm::vec3 wind{ 0.0f };       // m/s, world: air pushing on the faces it meets
    float gusts = 0.5f;           // 0 = steady wind .. 1 = strength and direction vary a lot
    uint32_t layer = 0;           // PhysicsSettings layer
};

// What PhysicsWorld::GetSoftBodyInfo tells of a soft body while it runs.
struct SoftBodyInfo
{
    size_t particles = 0, faces = 0, edges = 0, attachments = 0, pinned = 0;
    bool closed = false;          // every edge between two faces (it has an inside)
    float volume = 0.0f;          // m^3 now (closed meshes)
    float restVolume = 0.0f;
    size_t touching = 0;          // particles touching something
    bool active = false;          // awake
    glm::vec3 boundsMin{ 0.0f }, boundsMax{ 0.0f }; // world
};
