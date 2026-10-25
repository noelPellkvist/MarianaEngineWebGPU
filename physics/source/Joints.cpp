#include "WorldImpl.hpp"

#include <Mariana/Foundation/Logger.hpp>

#include <Jolt/Physics/Constraints/ConeConstraint.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
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
        void Add(const std::string& s)
        {
            for (char c : s)
                Add(c);
        }
    };

    // What the joint is made from; its motor, friction and break limits are set as it runs.
    void HashJoint(Hash& h, const Joint& j)
    {
        h.Add(j.type);
        h.Add(j.connected);
        h.Add(j.toParent);
        h.Add(j.collideConnected);
        h.Add(j.limited);
        h.Add(j.minLimit);
        h.Add(j.maxLimit);
        h.Add(j.connectedAnchor);
        h.Add(j.minDistance);
        h.Add(j.maxDistance);
        h.Add(j.springFrequency);
        h.Add(j.springDamping);
        h.Add(j.coneAngle);
        h.Add(j.twistMin);
        h.Add(j.twistMax);
        h.Add(j.lockedAxes);
    }

    JPH::SpringSettings Spring(const Joint& j)
    {
        return JPH::SpringSettings(JPH::ESpringMode::FrequencyAndDamping, std::max(j.springFrequency, 0.0f), std::max(j.springDamping, 0.0f));
    }

    JPH::EMotorState MotorState(JointMotor motor)
    {
        return motor == JointMotor::Velocity ? JPH::EMotorState::Velocity : motor == JointMotor::Position ? JPH::EMotorState::Position : JPH::EMotorState::Off;
    }
}

void PhysicsWorld::Impl::RemoveJoint(JointRecord& record)
{
    if (record.constraint)
        system->RemoveConstraint(record.constraint);
    record.constraint = nullptr;
    if (record.hasNoCollidePair)
        jointFilter->pairs.erase(record.noCollidePair);
    record.hasNoCollidePair = false;
}

void PhysicsWorld::Impl::DropJointsOf(JPH::BodyID body)
{
    for (auto it = joints.begin(); it != joints.end();)
    {
        if (it->second.body1 == body || (!it->second.body2.IsInvalid() && it->second.body2 == body))
        {
            RemoveJoint(it->second);
            it = joints.erase(it);
        }
        else
            ++it;
    }
}

// After the bodies: a constraint for every Joint whose bodies exist, made again when the joint, its
// place on its body or the bodies change.
void PhysicsWorld::Impl::SyncJoints()
{
    for (auto& [id, record] : joints)
        record.seen = false;

    const std::vector<Entity> withJoints = Find({ &jointFinder });
    // Entities by name (the first of each, in hierarchy order) for `connected`: the whole scene, so
    // only when a joint names what it holds on to.
    std::unordered_map<std::string, Entity> named;
    if (std::any_of(withJoints.begin(), withJoints.end(), [](Entity e) { const Joint* j = e.TryGet<Joint>(); return !j->toParent && !j->connected.empty(); }))
    {
        std::function<void(Entity)> gather = [&](Entity e) {
            named.emplace(e.GetName(), e);
            e.ForEachChild(gather);
        };
        scene.ForEachRoot([&](Entity root) { gather(root); });
    }

    // The body at or above an entity: the nearest one with a RigidBody, else its own static body.
    auto bodyOf = [&](Entity e, Entity& owner) {
        for (Entity a = e; a.IsValid(); a = scene.Parent(a))
        {
            if (a.TryGet<RigidBody>())
            {
                owner = a;
                const auto found = records.find(a.RawId());
                return found == records.end() ? JPH::BodyID() : found->second.body;
            }
        }
        owner = e;
        const auto found = records.find(e.RawId());
        return found == records.end() ? JPH::BodyID() : found->second.body;
    };

    for (Entity e : withJoints)
    {
        const Joint& j = *e.TryGet<Joint>();
        if (j.broken)
            continue; // until Stop puts it back
        Entity owner1, owner2;
        const JPH::BodyID body1 = bodyOf(e, owner1);
        if (body1.IsInvalid())
            continue;
        JPH::BodyID body2;
        if (j.toParent)
        {
            const Entity parent = scene.Parent(owner1);
            if (parent.IsValid())
            {
                body2 = bodyOf(parent, owner2);
                if (body2 == body1)
                    body2 = JPH::BodyID();
            }
        }
        else if (!j.connected.empty())
        {
            const auto other = named.find(j.connected);
            if (other == named.end())
                continue;
            body2 = bodyOf(other->second, owner2);
            if (body2.IsInvalid() || body2 == body1)
                continue;
        }

        // Where the joint is on its body: the LocalTRS chain from the body's entity down to it.
        glm::mat4 relative(1.0f);
        for (Entity a = e; a.IsValid() && a.RawId() != owner1.RawId(); a = scene.Parent(a))
            if (const LocalTRS* t = a.TryGet<LocalTRS>())
                relative = LocalMatrix(*t) * relative;
        Hash hash;
        HashJoint(hash, j);
        hash.Add(body1.GetIndexAndSequenceNumber());
        hash.Add(body2.IsInvalid() ? ~0u : body2.GetIndexAndSequenceNumber());
        hash.Add(relative);

        JointRecord& record = joints[e.RawId()];
        record.seen = true;
        if (record.constraint && record.signature == hash.value)
            continue;
        RemoveJoint(record);
        record = JointRecord();
        record.seen = true;
        record.signature = hash.value;
        record.body1 = body1;
        record.body2 = body2;

        // The joint's frame in the world now.
        glm::vec3 anchor;
        glm::quat rotation;
        SplitPose(WorldMatrix(scene, e), anchor, rotation);
        const JPH::RVec3 p(ToJolt(anchor));
        const JPH::Vec3 x = ToJolt(rotation * glm::vec3(1.0f, 0.0f, 0.0f)).Normalized();
        const JPH::Vec3 y = ToJolt(rotation * glm::vec3(0.0f, 1.0f, 0.0f)).Normalized();

        JPH::Ref<JPH::TwoBodyConstraintSettings> settings;
        switch (j.type)
        {
        case JointType::Fixed:
        {
            auto* s = new JPH::FixedConstraintSettings();
            s->mPoint1 = s->mPoint2 = p;
            s->mAxisX1 = s->mAxisX2 = x;
            s->mAxisY1 = s->mAxisY2 = y;
            settings = s;
            break;
        }
        case JointType::Point:
        {
            auto* s = new JPH::PointConstraintSettings();
            s->mPoint1 = s->mPoint2 = p;
            settings = s;
            break;
        }
        case JointType::Distance:
        {
            // Body 1 is the connected one (see below).
            auto* s = new JPH::DistanceConstraintSettings();
            s->mPoint2 = p;
            const glm::vec3 other = owner2.IsValid() ? glm::vec3(WorldMatrix(scene, owner2) * glm::vec4(j.connectedAnchor, 1.0f)) : j.connectedAnchor;
            s->mPoint1 = JPH::RVec3(ToJolt(other));
            s->mMinDistance = j.minDistance;
            s->mMaxDistance = j.maxDistance;
            s->mLimitsSpringSettings = Spring(j);
            settings = s;
            break;
        }
        case JointType::Hinge:
        {
            auto* s = new JPH::HingeConstraintSettings();
            s->mPoint1 = s->mPoint2 = p;
            s->mHingeAxis1 = s->mHingeAxis2 = x;
            s->mNormalAxis1 = s->mNormalAxis2 = y;
            if (j.limited)
            {
                s->mLimitsMin = glm::radians(std::clamp(std::min(j.minLimit, j.maxLimit), -180.0f, 0.0f));
                s->mLimitsMax = glm::radians(std::clamp(std::max(j.minLimit, j.maxLimit), 0.0f, 180.0f));
                s->mLimitsSpringSettings = Spring(j);
            }
            s->mMotorSettings = JPH::MotorSettings(std::max(j.motorFrequency, 0.01f), 1.0f, 0.0f, std::max(j.motorStrength, 0.0f));
            settings = s;
            break;
        }
        case JointType::Slider:
        {
            auto* s = new JPH::SliderConstraintSettings();
            s->mPoint1 = s->mPoint2 = p;
            s->mSliderAxis1 = s->mSliderAxis2 = x;
            s->mNormalAxis1 = s->mNormalAxis2 = y;
            if (j.limited)
            {
                s->mLimitsMin = std::min(std::min(j.minLimit, j.maxLimit), 0.0f);
                s->mLimitsMax = std::max(std::max(j.minLimit, j.maxLimit), 0.0f);
                s->mLimitsSpringSettings = Spring(j);
            }
            s->mMotorSettings = JPH::MotorSettings(std::max(j.motorFrequency, 0.01f), 1.0f, std::max(j.motorStrength, 0.0f), 0.0f);
            settings = s;
            break;
        }
        case JointType::Cone:
        {
            auto* s = new JPH::ConeConstraintSettings();
            s->mPoint1 = s->mPoint2 = p;
            s->mTwistAxis1 = s->mTwistAxis2 = x;
            s->mHalfConeAngle = glm::radians(std::clamp(j.coneAngle, 0.0f, 180.0f));
            settings = s;
            break;
        }
        case JointType::SwingTwist:
        {
            auto* s = new JPH::SwingTwistConstraintSettings();
            s->mPosition1 = s->mPosition2 = p;
            s->mTwistAxis1 = s->mTwistAxis2 = x;
            s->mPlaneAxis1 = s->mPlaneAxis2 = y;
            s->mNormalHalfConeAngle = s->mPlaneHalfConeAngle = glm::radians(std::clamp(j.coneAngle, 0.0f, 180.0f));
            s->mTwistMinAngle = glm::radians(std::clamp(std::min(j.twistMin, j.twistMax), -180.0f, 180.0f));
            s->mTwistMaxAngle = glm::radians(std::clamp(std::max(j.twistMin, j.twistMax), -180.0f, 180.0f));
            settings = s;
            break;
        }
        case JointType::SixDOF:
        {
            auto* s = new JPH::SixDOFConstraintSettings();
            s->mPosition1 = s->mPosition2 = p;
            s->mAxisX1 = s->mAxisX2 = x;
            s->mAxisY1 = s->mAxisY2 = y;
            for (int axis = 0; axis < 6; ++axis)
            {
                const auto a = static_cast<JPH::SixDOFConstraintSettings::EAxis>(axis);
                if ((j.lockedAxes >> axis) & 1)
                    s->MakeFixedAxis(a);
                else
                    s->MakeFreeAxis(a);
            }
            settings = s;
            break;
        }
        }

        // The connected body (or the world) is Jolt's body 1, so angles, positions and motors are the
        // joint's own body moving relative to what it holds on to.
        JPH::BodyInterface& bodies = system->GetBodyInterface();
        record.constraint = bodies.CreateConstraint(settings, body2, body1);
        if (!record.constraint)
            continue;
        system->AddConstraint(record.constraint);
        bodies.ActivateConstraint(record.constraint);
        if (!j.collideConnected && !body2.IsInvalid())
        {
            // The two bodies' groups carry their body index; the filter keeps the pair apart.
            for (JPH::BodyID body : { body1, body2 })
                bodies.SetCollisionGroup(body, JPH::CollisionGroup(jointFilter, body.GetIndex(), 0));
            record.noCollidePair = JointCollisionFilter::PairKey(body1.GetIndex(), body2.GetIndex());
            record.hasNoCollidePair = true;
            jointFilter->pairs.insert(record.noCollidePair);
        }
    }

    for (auto it = joints.begin(); it != joints.end();)
    {
        if (it->second.seen)
        {
            ++it;
            continue;
        }
        RemoveJoint(it->second);
        it = joints.erase(it);
    }
}

// Before each step: motors and friction as the components have them now.
void PhysicsWorld::Impl::DriveJoints()
{
    for (auto& [id, record] : joints)
    {
        if (!record.constraint)
            continue;
        const Entity e = scene.FromId(id);
        const Joint* j = e.IsValid() ? e.TryGet<Joint>() : nullptr;
        if (!j)
            continue;
        if (j->type == JointType::Hinge)
        {
            auto* hinge = static_cast<JPH::HingeConstraint*>(record.constraint.GetPtr());
            hinge->SetMaxFrictionTorque(std::max(j->friction, 0.0f));
            hinge->GetMotorSettings().SetTorqueLimit(std::max(j->motorStrength, 0.0f));
            hinge->GetMotorSettings().mSpringSettings.mFrequency = std::max(j->motorFrequency, 0.01f);
            hinge->SetMotorState(MotorState(j->motor));
            hinge->SetTargetAngularVelocity(glm::radians(j->motorSpeed));
            hinge->SetTargetAngle(glm::radians(j->motorTarget));
        }
        else if (j->type == JointType::Slider)
        {
            auto* slider = static_cast<JPH::SliderConstraint*>(record.constraint.GetPtr());
            slider->SetMaxFrictionForce(std::max(j->friction, 0.0f));
            slider->GetMotorSettings().SetForceLimit(std::max(j->motorStrength, 0.0f));
            slider->GetMotorSettings().mSpringSettings.mFrequency = std::max(j->motorFrequency, 0.01f);
            slider->SetMotorState(MotorState(j->motor));
            slider->SetTargetVelocity(j->motorSpeed);
            slider->SetTargetPosition(j->motorTarget);
        }
        if (j->motor != JointMotor::Off)
            system->GetBodyInterface().ActivateConstraint(record.constraint);
    }
}

// After each step: how hard each joint held (its impulses over the step), and those past their
// limits break.
void PhysicsWorld::Impl::CheckJoints(float dt)
{
    const float inverse = dt > 0.0f ? 1.0f / dt : 0.0f;
    for (auto it = joints.begin(); it != joints.end(); ++it)
    {
        JointRecord& record = it->second;
        if (!record.constraint)
            continue;
        Entity e = scene.FromId(it->first);
        Joint* j = e.IsValid() ? e.Get<Joint>() : nullptr;
        if (!j)
            continue;
        float position = 0.0f, rotation = 0.0f;
        JPH::TwoBodyConstraint* c = record.constraint.GetPtr();
        switch (j->type)
        {
        case JointType::Fixed: position = static_cast<JPH::FixedConstraint*>(c)->GetTotalLambdaPosition().Length(); rotation = static_cast<JPH::FixedConstraint*>(c)->GetTotalLambdaRotation().Length(); break;
        case JointType::Point: position = static_cast<JPH::PointConstraint*>(c)->GetTotalLambdaPosition().Length(); break;
        case JointType::Distance: position = std::abs(static_cast<JPH::DistanceConstraint*>(c)->GetTotalLambdaPosition()); break;
        case JointType::Hinge:
        {
            auto* hinge = static_cast<JPH::HingeConstraint*>(c);
            position = hinge->GetTotalLambdaPosition().Length();
            const JPH::Vector<2> r = hinge->GetTotalLambdaRotation();
            rotation = std::sqrt(r[0] * r[0] + r[1] * r[1]);
            break;
        }
        case JointType::Slider:
        {
            auto* slider = static_cast<JPH::SliderConstraint*>(c);
            const JPH::Vector<2> p = slider->GetTotalLambdaPosition();
            position = std::sqrt(p[0] * p[0] + p[1] * p[1]);
            rotation = slider->GetTotalLambdaRotation().Length();
            break;
        }
        case JointType::Cone: position = static_cast<JPH::ConeConstraint*>(c)->GetTotalLambdaPosition().Length(); break;
        case JointType::SwingTwist: position = static_cast<JPH::SwingTwistConstraint*>(c)->GetTotalLambdaPosition().Length(); break;
        case JointType::SixDOF: position = static_cast<JPH::SixDOFConstraint*>(c)->GetTotalLambdaPosition().Length(); rotation = static_cast<JPH::SixDOFConstraint*>(c)->GetTotalLambdaRotation().Length(); break;
        }
        record.force = position * inverse;
        record.torque = rotation * inverse;
        if ((j->breakForce > 0.0f && record.force > j->breakForce) || (j->breakTorque > 0.0f && record.torque > j->breakTorque))
        {
            j->broken = true;
            brokenJoints.push_back(e);
            system->GetBodyInterface().ActivateConstraint(record.constraint);
            RemoveJoint(record);
        }
    }
}

bool PhysicsWorld::GetJointInfo(Entity entity, JointInfo& info) const
{
    const Joint* j = entity.IsValid() ? entity.TryGet<Joint>() : nullptr;
    if (!j)
        return false;
    info = JointInfo();
    info.broken = j->broken;
    const auto found = m_Impl->joints.find(entity.RawId());
    if (found == m_Impl->joints.end() || !found->second.constraint)
        return true;
    const Impl::JointRecord& record = found->second;
    info.active = true;
    info.force = record.force;
    info.torque = record.torque;
    if (j->type == JointType::Hinge)
        info.position = glm::degrees(static_cast<const JPH::HingeConstraint*>(record.constraint.GetPtr())->GetCurrentAngle());
    else if (j->type == JointType::Slider)
        info.position = static_cast<const JPH::SliderConstraint*>(record.constraint.GetPtr())->GetCurrentPosition();
    else if (j->type == JointType::Distance)
    {
        const JPH::TwoBodyConstraint* c = record.constraint.GetPtr();
        const JPH::RVec3 a = c->GetBody1()->GetCenterOfMassTransform() * c->GetConstraintToBody1Matrix().GetTranslation();
        const JPH::RVec3 b = c->GetBody2()->GetCenterOfMassTransform() * c->GetConstraintToBody2Matrix().GetTranslation();
        info.position = static_cast<float>((a - b).Length());
    }
    return true;
}

const std::vector<Entity>& PhysicsWorld::BrokenJoints() const
{
    return m_Impl->brokenJoints;
}
