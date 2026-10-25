#include "WorldImpl.hpp"

#include <Mariana/Assets/AssetManager.hpp>
#include <Mariana/Foundation/Logger.hpp>
#include <Mariana/Geometry/MeshBuilder.hpp>
#include <Mariana/Rendering/Components.hpp>

#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/SoftBody/SoftBodyCreationSettings.h>
#include <Jolt/Physics/SoftBody/SoftBodyMotionProperties.h>
#include <Jolt/Physics/SoftBody/SoftBodySharedSettings.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <tuple>

using namespace PhysicsInternal;

namespace
{
    // XPBD compliance for a 0..1 stiffness: the constraint gives way up to ten times as much as its
    // two particles' inertia (0 = not at all), whatever their mass or the step; steep near 1, so
    // what is pumped up (pressure doesn't scale with mass) can still be held in shape.
    float Compliance(float stiffness, float invMassPair, float subStep)
    {
        const float give = 1.0f - std::clamp(stiffness, 0.0f, 1.0f);
        return 10.0f * give * give * give * invMassPair * subStep * subStep;
    }

    float SignedVolume(const std::vector<glm::vec3>& p, const std::vector<uint32_t>& faces)
    {
        float six = 0.0f;
        for (size_t f = 0; f + 2 < faces.size(); f += 3)
            six += glm::dot(glm::cross(p[faces[f]], p[faces[f + 1]]), p[faces[f + 2]]);
        return six / 6.0f;
    }

    // A mesh's data from its collision triangles (no texture coordinates): smooth normals.
    MeshData FromCollision(const MeshCollisionData& collision)
    {
        MeshData data;
        data.vertices.resize(collision.positions.size());
        std::vector<glm::vec3> normals(collision.positions.size(), glm::vec3(0.0f));
        for (size_t t = 0; t + 2 < collision.triangles.size(); t += 3)
        {
            const uint32_t a = collision.triangles[t], b = collision.triangles[t + 1], c = collision.triangles[t + 2];
            const glm::vec3 n = glm::cross(collision.positions[b] - collision.positions[a], collision.positions[c] - collision.positions[a]);
            normals[a] += n;
            normals[b] += n;
            normals[c] += n;
        }
        for (size_t i = 0; i < collision.positions.size(); ++i)
        {
            StandardVertex& v = data.vertices[i];
            v.position = collision.positions[i];
            v.normal = glm::length(normals[i]) > 0.0f ? glm::normalize(normals[i]) : glm::vec3(0.0f, 1.0f, 0.0f);
            v.color0 = glm::vec4(1.0f);
        }
        data.indices = collision.triangles;
        Submesh submesh;
        submesh.indexCount = static_cast<uint32_t>(data.indices.size());
        data.submeshes.push_back(submesh);
        return data;
    }

    // The particles a pin setting holds, by their place in the mesh's own space.
    std::vector<uint32_t> PinnedParticles(const std::vector<glm::vec3>& local, SoftBodyPin pin)
    {
        std::vector<uint32_t> pinned;
        if (pin == SoftBodyPin::None || local.empty())
            return pinned;
        Bounds bounds;
        for (const glm::vec3& p : local)
            bounds.Add(p);
        const glm::vec3 size = bounds.max - bounds.min;
        const float tolerance = 0.01f * std::max({ size.x, size.y, size.z }) + 1e-5f;
        if (pin == SoftBodyPin::Corners || pin == SoftBodyPin::TopCorners)
        {
            // The corners of its two widest axes (the third at its middle), nearest particle each.
            int axes[3] = { 0, 1, 2 };
            std::sort(axes, axes + 3, [&](int a, int b) { return size[a] > size[b]; });
            std::vector<uint32_t> corners;
            for (int i = 0; i < 4; ++i)
            {
                glm::vec3 corner = 0.5f * (bounds.min + bounds.max);
                corner[axes[0]] = (i & 1) ? bounds.max[axes[0]] : bounds.min[axes[0]];
                corner[axes[1]] = (i & 2) ? bounds.max[axes[1]] : bounds.min[axes[1]];
                uint32_t nearest = 0;
                for (uint32_t p = 1; p < local.size(); ++p)
                    if (glm::distance(local[p], corner) < glm::distance(local[nearest], corner))
                        nearest = p;
                corners.push_back(nearest);
            }
            if (pin == SoftBodyPin::TopCorners)
            {
                // The two highest (then furthest along +Z).
                std::sort(corners.begin(), corners.end(), [&](uint32_t a, uint32_t b) {
                    return local[a].y != local[b].y ? local[a].y > local[b].y : local[a].z > local[b].z;
                });
                corners.resize(2);
            }
            std::sort(corners.begin(), corners.end());
            corners.erase(std::unique(corners.begin(), corners.end()), corners.end());
            return corners;
        }
        const int axis = pin == SoftBodyPin::Top || pin == SoftBodyPin::Bottom ? 1 : pin == SoftBodyPin::Left || pin == SoftBodyPin::Right ? 0 : 2;
        const bool high = pin == SoftBodyPin::Top || pin == SoftBodyPin::Right || pin == SoftBodyPin::Front;
        for (uint32_t p = 0; p < local.size(); ++p)
            if (high ? local[p][axis] >= bounds.max[axis] - tolerance : local[p][axis] <= bounds.min[axis] + tolerance)
                pinned.push_back(p);
        return pinned;
    }
}

void PhysicsWorld::Impl::SyncSoftBodies()
{
    for (auto& [id, record] : softBodies)
        record.seen = false;

    for (Entity entity : Find<SoftBody>())
    {
        if (!entity.TryGet<MeshComponent>())
            continue;
        const SoftBody settings = *entity.TryGet<SoftBody>(); // a copy: making it again moves the entity's components
        MeshComponent& meshComponent = *entity.Get<MeshComponent>();
        SoftBodyRecord& record = softBodies[entity.RawId()];
        record.seen = true;
        // What it is made of: the entity's mesh (it draws a copy, its MeshOverride).
        const AssetHandle<IMesh> source = meshComponent.mesh;

        // Pressure and wind act as they are each step; anything else makes it again.
        Signature made;
        made.Add(source.Get().get());
        for (float f : { settings.mass, settings.stiffness, settings.bendStiffness, settings.friction, settings.restitution,
                         settings.damping, settings.gravityFactor, settings.thickness })
            made.Add(f);
        made.Add(settings.iterations);
        made.Add(settings.pin);
        made.Add(settings.layer);
        const uint64_t signature = made.value;
        if (record.made && record.signature == signature)
            continue;
        RemoveSoftBody(entity.RawId(), record);
        record.signature = signature;
        record.made = true; // tried, at least: a mesh it can't use isn't tried again every step
        BuildSoftBody(entity, settings, source, record);
    }

    for (auto it = softBodies.begin(); it != softBodies.end();)
    {
        if (it->second.seen)
        {
            ++it;
            continue;
        }
        RemoveSoftBody(it->first, it->second);
        it = softBodies.erase(it);
    }
}

void PhysicsWorld::Impl::BuildSoftBody(Entity entity, const SoftBody& settings, const AssetHandle<IMesh>& source, SoftBodyRecord& record)
{
    const IMesh* mesh = source.Get().get();
    AssetManager2* assets = source.Manager();
    MeshData data;
    if (mesh && mesh->source)
        data = *mesh->source;
    else if (mesh && mesh->collision)
    {
        data = FromCollision(*mesh->collision);
        if (warnedDynamicMesh.insert(entity.RawId()).second)
            Logger::Warning(std::string("Physics: ") + entity.GetName() + "'s mesh has no data kept on the CPU, so its soft body is drawn without texture coordinates");
    }
    if (data.Empty() || !assets)
    {
        if (warnedDynamicMesh.insert(entity.RawId()).second)
            Logger::Warning(std::string("Physics: ") + entity.GetName() + " has a SoftBody but no mesh with triangles to make one of");
        return;
    }

    // Vertices at the same place become one particle; triangle submeshes become its faces.
    PhysicsSoftBody drawn;
    std::map<std::tuple<int, int, int>, uint32_t> weld;
    std::vector<glm::vec3> local;
    drawn.particleOf.assign(data.vertices.size(), ~0u);
    drawn.faces.clear();
    drawn.triangles.clear();
    auto particle = [&](uint32_t vertex) {
        if (drawn.particleOf[vertex] != ~0u)
            return drawn.particleOf[vertex];
        const glm::vec3 p = data.vertices[vertex].position;
        const glm::ivec3 q = glm::ivec3(glm::round(p * 1e4f)); // a tenth of a millimetre
        const auto [it, added] = weld.try_emplace(std::make_tuple(q.x, q.y, q.z), static_cast<uint32_t>(local.size()));
        if (added)
            local.push_back(p);
        return drawn.particleOf[vertex] = it->second;
    };
    for (const Submesh& submesh : data.submeshes)
    {
        if (submesh.topology != PrimitiveTopology::Triangles)
            continue;
        for (uint32_t i = submesh.startIndex; i + 2 < submesh.startIndex + submesh.indexCount; i += 3)
        {
            const uint32_t a = data.indices[i], b = data.indices[i + 1], c = data.indices[i + 2];
            drawn.triangles.insert(drawn.triangles.end(), { a, b, c });
            const uint32_t pa = particle(a), pb = particle(b), pc = particle(c);
            if (pa != pb && pb != pc && pa != pc)
                drawn.faces.insert(drawn.faces.end(), { pa, pb, pc });
        }
    }
    if (drawn.faces.empty())
    {
        if (warnedDynamicMesh.insert(entity.RawId()).second)
            Logger::Warning(std::string("Physics: ") + entity.GetName() + " has a SoftBody but no mesh with triangles to make one of");
        return;
    }

    // Each drawn vertex's normal is the particles' faces around it that face its way (within 60
    // degrees of how it was modelled): smooth across seams, creases kept.
    std::vector<std::vector<uint32_t>> facesOf(local.size());
    std::vector<glm::vec3> restNormals(drawn.faces.size() / 3);
    for (uint32_t f = 0; f < drawn.faces.size() / 3; ++f)
    {
        const glm::vec3& a = local[drawn.faces[f * 3]];
        const glm::vec3& b = local[drawn.faces[f * 3 + 1]];
        const glm::vec3& c = local[drawn.faces[f * 3 + 2]];
        const glm::vec3 n = glm::cross(b - a, c - a);
        restNormals[f] = glm::length(n) > 0.0f ? glm::normalize(n) : glm::vec3(0.0f);
        for (int k = 0; k < 3; ++k)
            facesOf[drawn.faces[f * 3 + k]].push_back(f);
    }
    drawn.normalStart.assign(1, 0);
    drawn.normalFaces.clear();
    for (size_t v = 0; v < data.vertices.size(); ++v)
    {
        const uint32_t p = drawn.particleOf[v];
        if (p != ~0u)
        {
            const glm::vec3 modelled = data.vertices[v].normal;
            const size_t before = drawn.normalFaces.size();
            for (uint32_t f : facesOf[p])
                if (glm::dot(restNormals[f], modelled) > 0.5f)
                    drawn.normalFaces.push_back(f);
            if (drawn.normalFaces.size() == before)
                drawn.normalFaces.insert(drawn.normalFaces.end(), facesOf[p].begin(), facesOf[p].end());
        }
        drawn.normalStart.push_back(static_cast<uint32_t>(drawn.normalFaces.size()));
    }

    // Closed: every edge between exactly two faces.
    std::map<std::pair<uint32_t, uint32_t>, int> edgeUse;
    for (size_t f = 0; f + 2 < drawn.faces.size(); f += 3)
        for (int k = 0; k < 3; ++k)
        {
            const uint32_t a = drawn.faces[f + k], b = drawn.faces[f + (k + 1) % 3];
            ++edgeUse[{ std::min(a, b), std::max(a, b) }];
        }
    record.closed = std::all_of(edgeUse.begin(), edgeUse.end(), [](const auto& e) { return e.second == 2; });

    // Pins hold their place in the entity's space; the others share the mass.
    const std::vector<uint32_t> pinned = PinnedParticles(local, settings.pin);
    std::vector<bool> isPinned(local.size(), false);
    record.pins.clear();
    for (uint32_t p : pinned)
    {
        isPinned[p] = true;
        record.pins.push_back({ p, local[p] });
    }
    const size_t free = local.size() - pinned.size();
    const float invMass = free > 0 ? static_cast<float>(free) / std::max(settings.mass, 1e-4f) : 1.0f;
    record.invMass = invMass;

    const glm::mat4 world = WorldMatrix(scene, entity);
    std::vector<glm::vec3> placed(local.size());
    Bounds bounds;
    for (size_t p = 0; p < local.size(); ++p)
    {
        placed[p] = glm::vec3(world * glm::vec4(local[p], 1.0f));
        bounds.Add(placed[p]);
    }
    const glm::vec3 center = 0.5f * (bounds.min + bounds.max);

    JPH::Ref<JPH::SoftBodySharedSettings> shared = new JPH::SoftBodySharedSettings();
    for (size_t p = 0; p < placed.size(); ++p)
    {
        JPH::SoftBodySharedSettings::Vertex v;
        const glm::vec3 at = placed[p] - center;
        v.mPosition = JPH::Float3(at.x, at.y, at.z);
        v.mInvMass = isPinned[p] ? 0.0f : invMass;
        shared->mVertices.push_back(v);
    }
    // Jolt pushes a closed mesh out along its faces' normals: they must face out (positive volume).
    std::vector<glm::vec3> relative(placed.size());
    for (size_t p = 0; p < placed.size(); ++p)
        relative[p] = placed[p] - center;
    float volume = SignedVolume(relative, drawn.faces);
    const bool flip = volume < 0.0f;
    record.restVolume = std::abs(volume);
    // Pressure is a push of so many weights over its whole surface at its rest volume.
    float area = 0.0f;
    for (size_t f = 0; f + 2 < drawn.faces.size(); f += 3)
        area += 0.5f * glm::length(glm::cross(relative[drawn.faces[f + 1]] - relative[drawn.faces[f]], relative[drawn.faces[f + 2]] - relative[drawn.faces[f]]));
    const float gravity = glm::length(this->settings.gravity) > 0.0f ? glm::length(this->settings.gravity) : 9.81f;
    record.pressureScale = area > 0.0f ? std::max(settings.mass, 1e-4f) * gravity * record.restVolume / area : 0.0f;
    for (size_t f = 0; f + 2 < drawn.faces.size(); f += 3)
        shared->AddFace(flip ? JPH::SoftBodySharedSettings::Face(drawn.faces[f], drawn.faces[f + 2], drawn.faces[f + 1])
                             : JPH::SoftBodySharedSettings::Face(drawn.faces[f], drawn.faces[f + 1], drawn.faces[f + 2]));

    const uint32_t iterations = std::max(settings.iterations, 1u);
    const float subStep = std::max(this->settings.fixedTimestep, 1e-4f) / static_cast<float>(iterations);
    const float edge = Compliance(settings.stiffness, 2.0f * invMass, subStep);
    const float bend = settings.bendStiffness > 0.0f ? Compliance(settings.bendStiffness, 2.0f * invMass, subStep) : FLT_MAX;
    JPH::SoftBodySharedSettings::VertexAttributes attributes(edge, edge, bend,
        pinned.empty() ? JPH::SoftBodySharedSettings::ELRAType::None : JPH::SoftBodySharedSettings::ELRAType::EuclideanDistance, 1.0f);
    shared->CreateConstraints(&attributes, 1, settings.bendStiffness > 0.0f ? JPH::SoftBodySharedSettings::EBendType::Distance : JPH::SoftBodySharedSettings::EBendType::None);
    shared->Optimize();

    JPH::SoftBodyCreationSettings creation(shared, JPH::RVec3(ToJolt(center)), JPH::Quat::sIdentity(), MakeLayer(settings.layer, true));
    creation.mNumIterations = iterations;
    creation.mLinearDamping = std::max(settings.damping, 0.0f);
    creation.mFriction = std::max(settings.friction, 0.0f);
    creation.mRestitution = std::clamp(settings.restitution, 0.0f, 1.0f);
    creation.mPressure = std::max(settings.pressure, 0.0f) * record.pressureScale;
    creation.mGravityFactor = settings.gravityFactor;
    creation.mVertexRadius = std::max(settings.thickness, 0.0f);
    creation.mFacesDoubleSided = true;
    creation.mUserData = entity.RawId();
    record.body = system->GetBodyInterface().CreateAndAddSoftBody(creation, JPH::EActivation::Activate);
    if (record.body.IsInvalid())
        return;
    bodyEntities[record.body.GetIndexAndSequenceNumber()] = entity.RawId();

    // The entity draws a copy of its own instead (its MeshOverride), written as it moves.
    record.faces = drawn.faces;
    drawn.body = record.body.GetIndexAndSequenceNumber();
    drawn.vertices = data.vertices;
    std::shared_ptr<IMesh> copy = UploadMesh(std::move(data), MeshUsage::Dynamic);
    drawn.previous = drawn.current = placed;
    drawn.awake = true;
    drawn.written = false;
    entity.Add<PhysicsSoftBody>(drawn);
    entity.Add<MeshOverride>(MeshOverride{ assets->RegisterAsset<IMesh>(copy, AssetType::Mesh, entity.GetName() + std::string(" (soft body)")) });
}

void PhysicsWorld::Impl::RemoveSoftBody(uint64_t entityId, SoftBodyRecord& record)
{
    RemoveBody(record.body);
    // The entity draws its own mesh again.
    if (Entity entity = scene.FromId(entityId); entity.IsValid())
    {
        if (entity.TryGet<PhysicsSoftBody>())
            entity.Remove<PhysicsSoftBody>();
        if (entity.TryGet<MeshOverride>())
            entity.Remove<MeshOverride>();
    }
}

void PhysicsWorld::Impl::DriveSoftBodies(float dt)
{
    const JPH::BodyLockInterface& locks = system->GetBodyLockInterface();
    std::vector<JPH::BodyID> wake;
    for (auto& [id, record] : softBodies)
    {
        Entity entity = scene.FromId(id);
        const SoftBody* settings = entity.IsValid() ? entity.TryGet<SoftBody>() : nullptr;
        if (record.body.IsInvalid() || !settings)
            continue;
        JPH::BodyLockWrite lock(locks, record.body);
        if (!lock.Succeeded())
            continue;
        JPH::Body& body = lock.GetBody();
        auto* motion = static_cast<JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());
        motion->SetPressure(std::max(settings->pressure, 0.0f) * record.pressureScale);
        const JPH::RMat44 frame = body.GetCenterOfMassTransform();
        const JPH::RMat44 inverse = frame.InversedRotationTranslation();

        // Pins go where the entity has them by the end of the step.
        bool moved = false;
        if (!record.pins.empty())
        {
            const glm::mat4 world = WorldMatrix(scene, entity);
            for (const auto& [p, rest] : record.pins)
            {
                JPH::SoftBodyVertex& v = motion->GetVertex(p);
                const JPH::Vec3 target = JPH::Vec3(inverse * JPH::RVec3(ToJolt(glm::vec3(world * glm::vec4(rest, 1.0f)))));
                v.mVelocity = (target - v.mPosition) / dt;
                moved |= (target - v.mPosition).LengthSq() > 1e-8f;
            }
        }

        // Wind: air pushing on each face as much as it meets it (and a little along it), gusting.
        if (glm::length(settings->wind) > 0.0f)
        {
            const float t = waterTime;
            const float gusts = std::clamp(settings->gusts, 0.0f, 1.0f);
            const float strength = std::max(1.0f + gusts * (0.6f * std::sin(1.7f * t) + 0.4f * std::sin(4.3f * t + 2.0f)), 0.0f);
            const glm::vec3 wind = glm::vec3(glm::rotate(glm::mat4(1.0f), gusts * 0.35f * std::sin(0.7f * t), glm::vec3(0.0f, 1.0f, 0.0f)) * glm::vec4(settings->wind, 0.0f)) * strength;
            const glm::vec3 along = glm::normalize(wind);
            const glm::vec3 windLocal = ToGlm(inverse.Multiply3x3(ToJolt(wind)));
            JPH::Array<JPH::SoftBodyVertex>& vertices = motion->GetVertices();
            for (size_t f = 0; f + 2 < record.faces.size(); f += 3)
            {
                JPH::SoftBodyVertex& a = vertices[record.faces[f]];
                JPH::SoftBodyVertex& b = vertices[record.faces[f + 1]];
                JPH::SoftBodyVertex& c = vertices[record.faces[f + 2]];
                const glm::vec3 pa = ToGlm(a.mPosition), pb = ToGlm(b.mPosition), pc = ToGlm(c.mPosition);
                const glm::vec3 cross = glm::cross(pb - pa, pc - pa);
                const float area = 0.5f * glm::length(cross);
                if (area <= 0.0f)
                    continue;
                const glm::vec3 normal = cross / (2.0f * area);
                // Waves running down the wind.
                const glm::vec3 middle = ToGlm(JPH::Vec3(frame * JPH::RVec3(ToJolt((pa + pb + pc) / 3.0f))));
                const float ripple = 1.0f + gusts * 0.5f * std::sin(2.0f * glm::dot(middle, along) - 6.0f * t);
                const glm::vec3 air = windLocal * ripple - ToGlm(a.mVelocity + b.mVelocity + c.mVelocity) / 3.0f;
                const float facing = glm::dot(air, normal);
                const glm::vec3 tangent = air - normal * facing;
                const glm::vec3 force = 0.6f * area * (normal * facing * std::abs(facing) + 0.05f * tangent * glm::length(tangent));
                const JPH::Vec3 kick = ToJolt(force * dt / 3.0f);
                for (JPH::SoftBodyVertex* v : { &a, &b, &c })
                    v->mVelocity += kick * v->mInvMass;
            }
            moved = true;
        }
        if (moved && !body.IsActive())
            wake.push_back(record.body);
    }
    if (!wake.empty())
        system->GetBodyInterface().ActivateBodies(wake.data(), static_cast<int>(wake.size()));
}

void PhysicsWorld::Impl::TrackSoftBody(PhysicsSoftBody& drawn)
{
    if (drawn.body == JPH::BodyID::cInvalidBodyID)
        return;
    JPH::BodyLockRead lock(system->GetBodyLockInterface(), JPH::BodyID(drawn.body));
    if (!lock.Succeeded())
        return;
    const JPH::Body& body = lock.GetBody();
    const auto* motion = static_cast<const JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());
    const JPH::RMat44 frame = body.GetCenterOfMassTransform();
    const JPH::Array<JPH::SoftBodyVertex>& vertices = motion->GetVertices();
    drawn.previous.swap(drawn.current);
    drawn.current.resize(vertices.size());
    for (size_t p = 0; p < vertices.size(); ++p)
        drawn.current[p] = ToGlm(JPH::Vec3(frame * JPH::RVec3(vertices[p].mPosition)));
    drawn.awake = body.IsActive();
    if (drawn.awake)
        drawn.written = false;
}

void PhysicsWorld::Impl::ShowSoftBody(Entity entity, PhysicsSoftBody& drawn, IMesh* mesh, float alpha)
{
    if (!mesh || drawn.current.size() != drawn.previous.size() || (drawn.written && !drawn.awake))
        return;
    const glm::mat4 toLocal = glm::inverse(WorldMatrix(scene, entity));
    std::vector<glm::vec3>& positions = drawn.shown;
    positions.resize(drawn.current.size());
    for (size_t p = 0; p < positions.size(); ++p)
        positions[p] = glm::vec3(toLocal * glm::vec4(glm::mix(drawn.previous[p], drawn.current[p], alpha), 1.0f));

    std::vector<glm::vec3>& faceNormals = drawn.faceNormals;
    faceNormals.resize(drawn.faces.size() / 3);
    for (size_t f = 0; f < faceNormals.size(); ++f)
    {
        const glm::vec3& a = positions[drawn.faces[f * 3]];
        faceNormals[f] = glm::cross(positions[drawn.faces[f * 3 + 1]] - a, positions[drawn.faces[f * 3 + 2]] - a); // area weighted
    }
    Bounds bounds;
    for (size_t v = 0; v < drawn.vertices.size(); ++v)
    {
        const uint32_t p = drawn.particleOf[v];
        if (p == ~0u)
            continue;
        StandardVertex& vertex = drawn.vertices[v];
        vertex.position = positions[p];
        glm::vec3 normal(0.0f);
        for (uint32_t i = drawn.normalStart[v]; i < drawn.normalStart[v + 1]; ++i)
            normal += faceNormals[drawn.normalFaces[i]];
        if (glm::dot(normal, normal) > 1e-20f)
            vertex.normal = glm::normalize(normal);
        bounds.Add(vertex.position);
    }
    Geometry::GenerateTangents(drawn.vertices, drawn.triangles);
    mesh->WriteVertices(drawn.vertices.data(), drawn.vertices.size(), sizeof(StandardVertex), bounds);
    drawn.written = true;
}

bool PhysicsWorld::GetSoftBodyInfo(Entity entity, SoftBodyInfo& info) const
{
    const auto found = m_Impl->softBodies.find(entity.RawId());
    if (found == m_Impl->softBodies.end() || found->second.body.IsInvalid())
        return false;
    const Impl::SoftBodyRecord& record = found->second;
    JPH::BodyLockRead lock(m_Impl->system->GetBodyLockInterface(), record.body);
    if (!lock.Succeeded())
        return false;
    const JPH::Body& body = lock.GetBody();
    const auto* motion = static_cast<const JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());
    const JPH::SoftBodySharedSettings* shared = motion->GetSettings();
    info = SoftBodyInfo();
    info.particles = shared->mVertices.size();
    info.faces = shared->mFaces.size();
    info.edges = shared->mEdgeConstraints.size();
    info.attachments = shared->mLRAConstraints.size();
    info.pinned = record.pins.size();
    info.closed = record.closed;
    info.volume = motion->GetVolume();
    info.restVolume = record.restVolume;
    // Touching: pushed out of something during the last step, or resting on it (within a millimetre
    // of its thickness from the plane it collides with).
    const JPH::RMat44 frame = body.GetCenterOfMassTransform();
    const float radius = motion->GetVertexRadius();
    Bounds bounds;
    for (const JPH::SoftBodyVertex& v : motion->GetVertices())
    {
        const bool resting = v.mCollidingShapeIndex != -1 && v.mCollisionPlane.SignedDistance(v.mPosition) <= radius + 0.001f;
        info.touching += v.mHasContact || resting ? 1 : 0;
        bounds.Add(ToGlm(JPH::Vec3(frame * JPH::RVec3(v.mPosition))));
    }
    info.active = body.IsActive();
    info.boundsMin = bounds.min;
    info.boundsMax = bounds.max;
    return true;
}
