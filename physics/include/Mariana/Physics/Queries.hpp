#pragma once
#include <Mariana/Scene/ECS.hpp>

#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// What a ray or shape cast hit (PhysicsWorld::Raycast, ShapeCast).
struct RaycastHit
{
    Entity entity;              // the body's entity (the RigidBody's, or the collider's for static ones)
    glm::vec3 point{ 0.0f };    // world space
    glm::vec3 normal{ 0.0f };   // of the surface hit, pointing back toward the query
    float distance = 0.0f;      // along the direction from the origin
};

// Which bodies a query sees.
struct QueryFilter
{
    uint32_t layers = ~0u;      // a bit per PhysicsSettings layer
    bool triggers = false;      // also trigger colliders
    Entity ignore;              // one body to leave out (the one asking, say)
};

// A shape to cast or overlap with, centred on the query's position.
struct QueryShape
{
    enum class Type : uint8_t { Sphere, Box, Capsule };
    Type type = Type::Sphere;
    glm::vec3 halfExtents{ 0.5f }; // box
    float radius = 0.5f;           // sphere, capsule
    float halfHeight = 0.5f;       // capsule: of its straight part, along its local Y

    static QueryShape Sphere(float radius) { QueryShape s; s.type = Type::Sphere; s.radius = radius; return s; }
    static QueryShape Box(const glm::vec3& halfExtents) { QueryShape s; s.type = Type::Box; s.halfExtents = halfExtents; return s; }
    static QueryShape Capsule(float radius, float halfHeight) { QueryShape s; s.type = Type::Capsule; s.radius = radius; s.halfHeight = halfHeight; return s; }
};
