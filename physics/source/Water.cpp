#include "WorldImpl.hpp"

#include <Mariana/Rendering/Components.hpp>
#include <Mariana/Rendering/MaterialAsset.hpp>

#include <Jolt/Physics/Body/BodyLock.h>

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>

using namespace PhysicsInternal;

WaterSurface WaterSurfaceAt(const WaterVolume& water, const glm::vec3& top, const glm::vec3& up, const glm::vec3& right, const glm::vec3& forward,
                            const glm::vec3& point, float time)
{
    // Two waves across each other; the normal leans away from the slope.
    const glm::vec3 offset = point - top;
    const float x = glm::dot(offset, right), z = glm::dot(offset, forward);
    float height = 0.0f, dx = 0.0f, dz = 0.0f;
    if (water.waveHeight > 0.0f && water.waveLength > 1e-3f)
    {
        const float k = glm::two_pi<float>() / water.waveLength;
        const float omega = k * water.waveSpeed;
        const glm::vec2 d2 = glm::normalize(glm::vec2(0.35f, 1.0f));
        const float a1 = 0.6f * water.waveHeight, a2 = 0.4f * water.waveHeight, k2 = 1.7f * k;
        const float p1 = k * x - omega * time, p2 = k2 * (d2.x * x + d2.y * z) - 1.3f * omega * time;
        height = a1 * std::sin(p1) + a2 * std::sin(p2);
        dx = a1 * k * std::cos(p1) + a2 * k2 * d2.x * std::cos(p2);
        dz = a2 * k2 * d2.y * std::cos(p2);
    }
    WaterSurface surface;
    surface.point = point - up * (glm::dot(offset, up) - height);
    surface.normal = glm::normalize(up - right * dx - forward * dz);
    return surface;
}

void PhysicsWorld::Impl::AddWater(Entity entity, const glm::mat4& world, const WaterVolume& water)
{
    WaterRecord record;
    record.entity = entity.RawId();
    record.water = water;
    const glm::vec3 scale(glm::length(glm::vec3(world[0])), glm::length(glm::vec3(world[1])), glm::length(glm::vec3(world[2])));
    record.right = scale.x > 1e-6f ? glm::vec3(world[0]) / scale.x : glm::vec3(1.0f, 0.0f, 0.0f);
    record.up = scale.y > 1e-6f ? glm::vec3(world[1]) / scale.y : glm::vec3(0.0f, 1.0f, 0.0f);
    record.forward = scale.z > 1e-6f ? glm::vec3(world[2]) / scale.z : glm::vec3(0.0f, 0.0f, 1.0f);
    record.half = 0.5f * glm::abs(water.size) * scale;
    record.center = glm::vec3(world[3]);
    record.top = record.center + record.up * record.half.y;
    // The box's world bounds, to find what may be in it quickly.
    const glm::vec3 reach = glm::abs(record.right) * record.half.x + glm::abs(record.up) * record.half.y + glm::abs(record.forward) * record.half.z;
    record.boundsMin = record.center - reach;
    record.boundsMax = record.center + reach;
    waters.push_back(record);
}

const PhysicsWorld::Impl::WaterRecord* PhysicsWorld::Impl::WaterAt(const glm::vec3& point) const
{
    for (const WaterRecord& w : waters)
    {
        const glm::vec3 offset = point - w.center;
        if (std::abs(glm::dot(offset, w.right)) <= w.half.x && std::abs(glm::dot(offset, w.forward)) <= w.half.z &&
            std::abs(glm::dot(offset, w.up)) <= w.half.y + std::max(w.water.waveHeight, 0.0f))
            return &w;
    }
    return nullptr;
}

void PhysicsWorld::Impl::ApplyBuoyancy(float dt)
{
    if (waters.empty())
        return;
    JPH::BodyInterface& bodies = system->GetBodyInterface();
    const JPH::Vec3 gravity = system->GetGravity();
    // Bodies in entity order, so threads or hashing never change what is done first.
    std::vector<uint64_t> order;
    for (const auto& [id, record] : records)
        if (record.motion == MotionType::Dynamic && !record.body.IsInvalid())
            order.push_back(id);
    std::sort(order.begin(), order.end());
    for (uint64_t id : order)
    {
        const Record& record = records[id];
        const RigidBody* rigidBody = scene.FromId(id).TryGet<RigidBody>();
        const float scale = rigidBody ? rigidBody->buoyancy : 1.0f;
        if (scale <= 0.0f)
            continue;

        bool wake = false;
        {
            JPH::BodyLockWrite lock(system->GetBodyLockInterface(), record.body);
            if (!lock.Succeeded())
                continue;
            JPH::Body& body = lock.GetBody();
            const JPH::AABox bounds = body.GetWorldSpaceBounds();
            const glm::vec3 boundsMin = ToGlm(bounds.mMin), boundsMax = ToGlm(bounds.mMax);
            const glm::vec3 position = ToGlm(JPH::Vec3(body.GetCenterOfMassPosition()));
            JPH::MotionProperties* motion = body.GetMotionProperties();
            if (motion->GetInverseMass() <= 0.0f)
                continue;
            for (const WaterRecord& w : waters)
            {
                if (glm::any(glm::lessThan(boundsMax, w.boundsMin)) || glm::any(glm::greaterThan(boundsMin, w.boundsMax)))
                    continue;
                // Within its sides: the body's center over the water.
                const glm::vec3 offset = position - w.center;
                if (std::abs(glm::dot(offset, w.right)) > w.half.x || std::abs(glm::dot(offset, w.forward)) > w.half.z)
                    continue;
                const WaterSurface surface = WaterSurfaceAt(w.water, w.top, w.up, w.right, w.forward, position, waterTime);
                float total = 0.0f, submerged = 0.0f;
                JPH::Vec3 centerOfBuoyancy;
                body.GetSubmergedVolume(JPH::RVec3(ToJolt(surface.point)), ToJolt(surface.normal), total, submerged, centerOfBuoyancy);
                if (submerged <= 0.0f || total <= 0.0f)
                    break;
                // A sleeping body in still water stays asleep.
                if (!body.IsActive() && w.water.waveHeight <= 0.0f && w.water.flow == glm::vec3(0.0f))
                    break;
                // Jolt's buoyancy is the water's density over the body's own.
                const float buoyancy = w.water.density * total * motion->GetInverseMass() * scale;
                body.ApplyBuoyancyImpulse(total, submerged, centerOfBuoyancy, buoyancy, std::max(w.water.linearDrag, 0.0f),
                                          std::max(w.water.angularDrag, 0.0f), ToJolt(w.water.flow), gravity, dt);
                // Water also calms what floats in it (waves carry the energy away), which the drag
                // barely does at low speeds.
                const float keep = std::max(0.0f, 1.0f - std::max(w.water.damping, 0.0f) * (submerged / total) * dt);
                const JPH::Vec3 flow = ToJolt(w.water.flow);
                motion->SetLinearVelocityClamped(flow + (motion->GetLinearVelocity() - flow) * keep);
                motion->SetAngularVelocityClamped(motion->GetAngularVelocity() * keep);
                wake = !body.IsActive();
                break;
            }
        }
        if (wake)
            bodies.ActivateBody(record.body);
    }
}

bool PhysicsWorld::WaterAt(const glm::vec3& point, WaterSurface& surface) const
{
    const Impl::WaterRecord* w = m_Impl->WaterAt(point);
    if (!w)
        return false;
    surface = WaterSurfaceAt(w->water, w->top, w->up, w->right, w->forward, point, m_Impl->waterTime);
    return true;
}

void PhysicsWorld::Impl::RippleWater()
{
    for (const WaterRecord& w : waters)
    {
        Entity water = scene.FromId(w.entity);
        if (!water.IsValid())
            continue;
        const glm::vec2 offset = glm::fract(w.water.rippleSpeed * waterTime);
        auto ripple = [&](Entity e) {
            const MeshComponent* mesh = e.TryGet<MeshComponent>();
            if (!mesh)
                return;
            for (const AssetHandle<MaterialAsset>& handle : mesh->materials)
            {
                MaterialAsset* material = handle.Get().get();
                if (!material || !material->GetTexture(MaterialTexture::Normal).texture.IsValid())
                    continue;
                MaterialAsset::TextureTransform transform = material->GetTexture(MaterialTexture::Normal).transform;
                if (transform.offset == offset)
                    continue;
                transform.offset = offset;
                material->SetTextureTransform(MaterialTexture::Normal, transform);
            }
        };
        ripple(water);
        scene.ForEachDescendant(water, ripple);
    }
}
