#include "WorldImpl.hpp"

#include <Mariana/Foundation/Logger.hpp>

#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

#include <glm/gtc/constants.hpp>

#include <cmath>
#include <functional>

using namespace PhysicsInternal;

namespace
{
    struct Hash
    {
        uint64_t value = 1469598103934665603ull;
        template <typename T>
        void Add(const T& v)
        {
            const auto* bytes = reinterpret_cast<const unsigned char*>(&v);
            for (size_t i = 0; i < sizeof(T); ++i)
                value = (value ^ bytes[i]) * 1099511628211ull;
        }
    };

    // What the character is made from (not how it is steered).
    uint64_t Signature(const CharacterController& c)
    {
        Hash h;
        for (float v : { c.radius, c.height, c.maxSlope, c.mass, c.strength })
            h.Add(v);
        h.Add(c.layer);
        return h.value;
    }

    float YawOf(const glm::quat& q)
    {
        const glm::vec3 forward = q * glm::vec3(0.0f, 0.0f, 1.0f);
        return std::atan2(forward.x, forward.z);
    }

    glm::quat Yaw(float yaw) { return glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f)); }
}

namespace PhysicsInternal
{
    // Only triggers: walking into a body is the character's business (and its inner body reports
    // what hits it).
    void CharacterContacts::OnContactAdded(const JPH::CharacterVirtual* character, const JPH::CharacterContact& contact, JPH::CharacterContactSettings&)
    {
        if (!collector || !contact.mIsSensorB || contact.mBodyB.IsInvalid())
            return;
        ContactCollector::Added added;
        const JPH::BodyID inner = character->GetInnerBodyID();
        const bool innerFirst = inner.GetIndexAndSequenceNumber() < contact.mBodyB.GetIndexAndSequenceNumber();
        added.body1 = innerFirst ? inner : contact.mBodyB;
        added.body2 = innerFirst ? contact.mBodyB : inner;
        added.subShapes = CharacterContactKey(contact.mSubShapeIDB);
        added.entityA = contact.mUserData;
        added.entityB = character->GetUserData();
        added.trigger = true;
        added.point = ToGlm(JPH::Vec3(contact.mPosition));
        added.normal = ToGlm(-contact.mContactNormal); // from the trigger toward the character
        added.speed = std::max(-character->GetLinearVelocity().Dot(contact.mContactNormal), 0.0f);
        collector->Add(added);
    }

    void CharacterContacts::OnContactRemoved(const JPH::CharacterVirtual* character, const JPH::BodyID& body, const JPH::SubShapeID& subShape)
    {
        if (!collector)
            return;
        const JPH::BodyID inner = character->GetInnerBodyID();
        const bool innerFirst = inner.GetIndexAndSequenceNumber() < body.GetIndexAndSequenceNumber();
        collector->Remove(ContactCollector::Removed{ innerFirst ? inner : body, innerFirst ? body : inner, CharacterContactKey(subShape) });
    }
}

void PhysicsWorld::Impl::RemoveCharacter(CharacterRecord& record)
{
    if (!record.character)
        return;
    characterCollision->Remove(record.character);
    if (!record.innerBody.IsInvalid())
        removedBodies.push_back(record.innerBody.GetIndexAndSequenceNumber());
    record.character = nullptr; // removes its inner body
    record.innerBody = JPH::BodyID();
}

// After the bodies: a CharacterVirtual for every CharacterController, made again when its shape or
// weight changes; one moved by someone else (the editor, a script) is put there.
void PhysicsWorld::Impl::SyncCharacters()
{
    for (auto& [id, record] : characters)
        record.seen = false;

    std::function<void(Entity)> visit = [&](Entity e) {
        e.ForEachChild(visit);
        const CharacterController* c = e.TryGet<CharacterController>();
        const LocalTRS* trs = e.TryGet<LocalTRS>();
        if (!c || !trs)
            return;
        if (e.TryGet<RigidBody>())
        {
            if (warnedDynamicMesh.insert(e.RawId()).second)
                Logger::Warning(std::string("Physics: ") + e.GetName() + " has a CharacterController and a RigidBody; the character is left out");
            return;
        }
        glm::vec3 position;
        glm::quat rotation;
        SplitPose(WorldMatrix(scene, e), position, rotation);

        CharacterRecord& record = characters[e.RawId()];
        record.seen = true;
        const uint64_t signature = Signature(*c);
        if (record.character && record.signature == signature)
        {
            if (trs->version != record.writtenVersion)
            {
                record.character->SetPosition(JPH::RVec3(ToJolt(position)));
                record.yaw = YawOf(rotation);
                record.character->SetRotation(ToJolt(Yaw(record.yaw)));
                record.writtenVersion = trs->version;
            }
            return;
        }

        JPH::Vec3 velocity = JPH::Vec3::sZero();
        if (record.character)
            velocity = record.character->GetLinearVelocity();
        RemoveCharacter(record);

        // A capsule standing on the entity's origin.
        const float radius = std::max(c->radius, 0.01f);
        const float halfCylinder = std::max(0.5f * c->height - radius, 0.01f);
        JPH::RotatedTranslatedShapeSettings shapeSettings(JPH::Vec3(0.0f, halfCylinder + radius, 0.0f), JPH::Quat::sIdentity(), new JPH::CapsuleShape(halfCylinder, radius));
        const JPH::ShapeSettings::ShapeResult shape = shapeSettings.Create();
        if (shape.HasError())
            return;

        JPH::Ref<JPH::CharacterVirtualSettings> settings = new JPH::CharacterVirtualSettings();
        settings->mShape = shape.Get();
        settings->mMaxSlopeAngle = glm::radians(std::clamp(c->maxSlope, 0.0f, 89.0f));
        settings->mMass = std::max(c->mass, 0.1f);
        settings->mMaxStrength = std::max(c->strength, 0.0f);
        settings->mBackFaceMode = JPH::EBackFaceMode::CollideWithBackFaces;
        // Only the bottom of the capsule stands on things.
        settings->mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -radius);
        settings->mInnerBodyShape = shape.Get();
        settings->mInnerBodyLayer = MakeLayer(c->layer, true);

        record.yaw = YawOf(rotation);
        record.character = new JPH::CharacterVirtual(settings, JPH::RVec3(ToJolt(position)), ToJolt(Yaw(record.yaw)), e.RawId(), system.get());
        record.character->SetListener(&characterContacts);
        record.character->SetCharacterVsCharacterCollision(characterCollision.get());
        record.character->SetLinearVelocity(velocity);
        characterCollision->Add(record.character);
        record.innerBody = record.character->GetInnerBodyID();
        if (!record.innerBody.IsInvalid())
            bodyEntities[record.innerBody.GetIndexAndSequenceNumber()] = e.RawId();
        record.signature = signature;
        record.writtenVersion = trs->version;
    };
    scene.ForEachRoot([&](Entity root) { visit(root); });

    for (auto it = characters.begin(); it != characters.end();)
    {
        if (it->second.seen)
        {
            ++it;
            continue;
        }
        RemoveCharacter(it->second);
        it = characters.erase(it);
    }
}

// Before each step: the characters move (Jolt's ExtendedUpdate: move, walk up stairs, stick to the
// floor), steered by their controllers.
void PhysicsWorld::Impl::MoveCharacters(float dt)
{
    const JPH::Vec3 up = JPH::Vec3::sAxisY();
    for (auto& [id, record] : characters)
    {
        Entity e = scene.FromId(id);
        CharacterController* c = e.IsValid() ? e.Get<CharacterController>() : nullptr;
        if (!c || !record.character)
            continue;
        JPH::CharacterVirtual& character = *record.character;
        const JPH::Vec3 gravity = system->GetGravity() * c->gravityFactor;

        glm::vec3 move = c->move;
        move.y = 0.0f;
        if (glm::length(move) > 1.0f)
            move = glm::normalize(move);
        const JPH::Vec3 wanted = ToJolt(move * c->speed * (c->run ? c->runFactor : 1.0f));

        character.UpdateGroundVelocity();
        const JPH::Vec3 current = character.GetLinearVelocity();
        const JPH::Vec3 vertical = up * current.Dot(up);
        const JPH::Vec3 ground = character.GetGroundVelocity();
        const bool towardGround = (vertical.GetY() - ground.GetY()) < 0.1f;
        JPH::Vec3 velocity;
        if (character.GetGroundState() == JPH::CharacterBase::EGroundState::OnGround && towardGround)
        {
            // On the ground: go with it, walk, maybe jump.
            velocity = ground + wanted;
            if (c->jump)
                velocity += up * c->jumpSpeed;
        }
        else
        {
            // In the air (or sliding down something too steep): keep going, steer a little.
            JPH::Vec3 horizontal = current - vertical;
            const float control = std::clamp(c->airControl, 0.0f, 1.0f);
            horizontal += (wanted - horizontal) * std::min(1.0f, control * 10.0f * dt);
            velocity = vertical + horizontal;
        }
        velocity += gravity * dt;
        character.SetLinearVelocity(velocity);

        if (c->turnToMove && glm::length(move) > 0.1f)
        {
            const float target = std::atan2(move.x, move.z);
            float delta = std::remainder(target - record.yaw, glm::two_pi<float>());
            const float most = glm::radians(std::max(c->turnSpeed, 0.0f)) * dt;
            record.yaw += std::clamp(delta, -most, most);
            character.SetRotation(ToJolt(Yaw(record.yaw)));
        }

        JPH::CharacterVirtual::ExtendedUpdateSettings update;
        update.mStickToFloorStepDown = JPH::Vec3(0.0f, -std::max(c->stickDown, 0.0f), 0.0f);
        update.mWalkStairsStepUp = JPH::Vec3(0.0f, std::max(c->stepHeight, 0.0f), 0.0f);
        const JPH::ObjectLayer layer = MakeLayer(c->layer, true);
        character.ExtendedUpdate(dt, gravity, update, system->GetDefaultBroadPhaseLayerFilter(layer), system->GetDefaultLayerFilter(layer), {}, {}, *temp);
    }
}

// After the steps: each entity where its character is; jumps are spent.
void PhysicsWorld::Impl::WriteCharacters()
{
    for (auto& [id, record] : characters)
    {
        Entity e = scene.FromId(id);
        LocalTRS* trs = e.IsValid() ? e.Get<LocalTRS>() : nullptr;
        if (!trs || !record.character)
            continue;
        if (CharacterController* c = e.Get<CharacterController>())
            c->jump = false;
        SetWorldPose(scene, e, *trs, ToGlm(JPH::Vec3(record.character->GetPosition())), Yaw(record.yaw));
        record.writtenVersion = trs->version;
    }
}

bool PhysicsWorld::GetCharacterInfo(Entity entity, CharacterInfo& info) const
{
    const auto found = m_Impl->characters.find(entity.RawId());
    if (found == m_Impl->characters.end() || !found->second.character)
        return false;
    const JPH::CharacterVirtual& character = *found->second.character;
    info.grounded = character.GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;
    info.onSteepGround = character.GetGroundState() == JPH::CharacterBase::EGroundState::OnSteepGround;
    info.velocity = ToGlm(character.GetLinearVelocity());
    info.groundNormal = ToGlm(character.GetGroundNormal());
    info.ground = character.GetGroundBodyID().IsInvalid() ? Entity{} : m_Impl->EntityOf(character.GetGroundBodyID());
    return true;
}
