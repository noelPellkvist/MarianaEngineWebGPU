#include "WorldImpl.hpp"

#include <Mariana/Foundation/Logger.hpp>

#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Vehicle/MotorcycleController.h>
#include <Jolt/Physics/Vehicle/TrackedVehicleController.h>
#include <Jolt/Physics/Vehicle/VehicleCollisionTester.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cfloat>
#include <functional>

using namespace PhysicsInternal;

namespace
{
    // What a wheel stands on: anything but its own vehicle and soft bodies (Jolt's vehicle would push
    // a soft body about as if it were rigid; the chassis still bumps into them).
    class WheelBodyFilter final : public JPH::BodyFilter
    {
    public:
        explicit WheelBodyFilter(JPH::BodyID vehicle) : m_Vehicle(vehicle) {}
        bool ShouldCollide(const JPH::BodyID& body) const override { return body != m_Vehicle; }
        bool ShouldCollideLocked(const JPH::Body& body) const override { return !body.IsSoftBody(); }

    private:
        JPH::BodyID m_Vehicle;
    };

    // A vehicle's settings, not what the driver does with it.
    void HashVehicle(Signature& h, const Vehicle& v)
    {
        h.Add(v.type);
        h.Add(v.collision);
        h.Add(v.drive);
        h.Add(v.engineTorque);
        h.Add(v.minRPM);
        h.Add(v.maxRPM);
        h.Add(v.automatic);
        h.Add(v.shiftUpRPM);
        h.Add(v.shiftDownRPM);
        h.Add(v.clutchStrength);
        for (float ratio : v.gearRatios)
            h.Add(ratio);
        h.Add(v.reverseRatio);
        h.Add(v.differentialRatio);
        h.Add(v.limitedSlipRatio);
        h.Add(v.antiRollStiffness);
        h.Add(v.maxPitchRollAngle);
        h.Add(v.maxLeanAngle);
    }

    // A wheel as the vehicle has it: where it hangs with the suspension out (body space) and its
    // rest orientation there.
    struct WheelPlacement
    {
        Entity entity;
        VehicleWheel settings;
        glm::vec3 position{ 0.0f };
        glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
    };

    // Wheels side by side: one axle (frontmost first), its wheels left (+X) to right.
    std::vector<std::vector<int>> Axles(const std::vector<WheelPlacement>& wheels)
    {
        std::vector<int> order(wheels.size());
        for (size_t i = 0; i < wheels.size(); ++i)
            order[i] = static_cast<int>(i);
        std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return wheels[a].position.z > wheels[b].position.z; });
        std::vector<std::vector<int>> axles;
        for (int i : order)
        {
            if (axles.empty() || std::abs(wheels[axles.back().front()].position.z - wheels[i].position.z) > 0.2f)
                axles.emplace_back();
            axles.back().push_back(i);
        }
        for (std::vector<int>& axle : axles)
            std::stable_sort(axle.begin(), axle.end(), [&](int a, int b) { return wheels[a].position.x > wheels[b].position.x; });
        return axles;
    }

    // The suspension points down, leaning forward at the bottom by the caster angle.
    JPH::Vec3 SuspensionDirection(const VehicleWheel& w)
    {
        return JPH::Vec3(0.0f, -1.0f, std::tan(glm::radians(w.casterAngle))).Normalized();
    }

    void ScaleCurve(JPH::LinearCurve& curve, float factor)
    {
        for (JPH::LinearCurve::Point& point : curve.mPoints)
            point.mY *= factor;
    }

    template <typename Controller>
    void SetupDrivetrain(Controller& c, const Vehicle& v)
    {
        c.mEngine.mMaxTorque = std::max(v.engineTorque, 0.0f);
        c.mEngine.mMinRPM = std::max(v.minRPM, 1.0f);
        c.mEngine.mMaxRPM = std::max(v.maxRPM, c.mEngine.mMinRPM + 1.0f);
        c.mTransmission.mMode = v.automatic ? JPH::ETransmissionMode::Auto : JPH::ETransmissionMode::Manual;
        c.mTransmission.mShiftUpRPM = v.shiftUpRPM;
        c.mTransmission.mShiftDownRPM = v.shiftDownRPM;
        c.mTransmission.mClutchStrength = std::max(v.clutchStrength, 0.0f);
        if (!v.gearRatios.empty())
            c.mTransmission.mGearRatios.assign(v.gearRatios.begin(), v.gearRatios.end());
        c.mTransmission.mReverseGearRatios = { v.reverseRatio < 0.0f ? v.reverseRatio : -std::abs(v.reverseRatio) };
    }

}

void PhysicsWorld::Impl::RemoveVehicle(VehicleRecord& record)
{
    if (!record.constraint)
        return;
    for (const VehicleRecord::Wheel& wheel : record.wheels)
        if (Entity e = scene.FromId(wheel.entity); e.IsValid() && e.TryGet<PhysicsWheel>())
            e.Remove<PhysicsWheel>();
    system->RemoveStepListener(record.constraint);
    system->RemoveConstraint(record.constraint);
    record.constraint = nullptr;
}

// After the bodies: a vehicle constraint for every chassis with a Vehicle, made again when its
// settings, wheels or body change.
void PhysicsWorld::Impl::SyncVehicles()
{
    for (auto& [id, record] : vehicles)
        record.seen = false;

    auto sync = [&](Entity chassis) {
        const Vehicle* vehicle = chassis.TryGet<Vehicle>();
        const auto body = records.find(chassis.RawId());
        if (!vehicle || body == records.end() || body->second.motion != MotionType::Dynamic || body->second.body.IsInvalid())
            return;

        // The wheels below the chassis (not below another body), placed in the chassis's body space
        // at their rest poses: from the LocalTRS values below it and the scale on and above it (not
        // from world matrices, whose rounding changes as the vehicle moves).
        glm::vec3 chassisScale(1.0f);
        for (Entity e = chassis; e.IsValid(); e = scene.Parent(e))
            if (const LocalTRS* t = e.TryGet<LocalTRS>())
                chassisScale *= glm::vec3(t->scl[0], t->scl[1], t->scl[2]);
        const glm::mat4 toBody = glm::scale(glm::mat4(1.0f), chassisScale);
        std::vector<WheelPlacement> wheels;
        std::function<void(Entity, const glm::mat4&)> gather = [&](Entity e, const glm::mat4& parent) {
            if (e.TryGet<RigidBody>())
                return;
            const LocalTRS* trs = e.TryGet<LocalTRS>();
            LocalTRS local = trs ? *trs : LocalTRS{};
            const VehicleWheel* wheel = e.TryGet<VehicleWheel>();
            if (wheel && trs)
            {
                WheelRest& rest = wheelRests[e.RawId()];
                if (trs->version != rest.writtenVersion)
                {
                    rest.position = glm::vec3(trs->pos[0], trs->pos[1], trs->pos[2]);
                    rest.rotation = glm::quat(trs->rot_quat[3], trs->rot_quat[0], trs->rot_quat[1], trs->rot_quat[2]);
                    rest.writtenVersion = ~0u;
                }
                for (int i = 0; i < 3; ++i)
                    local.pos[i] = rest.position[i];
                local.rot_quat[0] = rest.rotation.x;
                local.rot_quat[1] = rest.rotation.y;
                local.rot_quat[2] = rest.rotation.z;
                local.rot_quat[3] = rest.rotation.w;
            }
            const glm::mat4 m = parent * (trs ? LocalMatrix(local) : glm::mat4(1.0f));
            if (wheel)
            {
                WheelPlacement placement;
                placement.entity = e;
                placement.settings = *wheel;
                SplitPose(m, placement.position, placement.rotation);
                wheels.push_back(placement);
                return;
            }
            e.ForEachChild([&](Entity child) { gather(child, m); });
        };
        chassis.ForEachChild([&](Entity child) { gather(child, toBody); });
        std::stable_sort(wheels.begin(), wheels.end(), [](const WheelPlacement& a, const WheelPlacement& b) { return a.entity.RawId() < b.entity.RawId(); });

        Signature hash;
        HashVehicle(hash, *vehicle);
        hash.Add(body->second.body.GetIndexAndSequenceNumber());
        for (const WheelPlacement& w : wheels)
        {
            hash.Add(w.entity.RawId());
            hash.Add(w.settings);
            hash.Add(w.position);
            hash.Add(w.rotation);
        }

        VehicleRecord& record = vehicles[chassis.RawId()];
        record.seen = true;
        if (record.constraint && record.signature == hash.value)
            return;
        RemoveVehicle(record);
        record = VehicleRecord();
        record.seen = true;
        record.signature = hash.value;
        record.body = body->second.body;
        record.type = vehicle->type;
        if (wheels.empty())
            return;

        JPH::VehicleConstraintSettings settings;
        settings.mUp = JPH::Vec3::sAxisY();
        settings.mForward = JPH::Vec3::sAxisZ();
        settings.mMaxPitchRollAngle = glm::radians(std::clamp(vehicle->maxPitchRollAngle, 0.0f, 180.0f));
        const bool tracked = vehicle->type == VehicleType::Tracked;
        for (const WheelPlacement& w : wheels)
        {
            JPH::Ref<JPH::WheelSettings> ws;
            if (tracked)
            {
                auto* tv = new JPH::WheelSettingsTV();
                tv->mLongitudinalFriction *= w.settings.grip;
                tv->mLateralFriction *= w.settings.grip;
                ws = tv;
            }
            else
            {
                auto* wv = new JPH::WheelSettingsWV();
                wv->mMaxSteerAngle = glm::radians(std::clamp(w.settings.maxSteerAngle, 0.0f, 89.0f));
                wv->mMaxBrakeTorque = std::max(w.settings.maxBrakeTorque, 0.0f);
                wv->mMaxHandBrakeTorque = std::max(w.settings.maxHandBrakeTorque, 0.0f);
                ScaleCurve(wv->mLongitudinalFriction, w.settings.grip);
                ScaleCurve(wv->mLateralFriction, w.settings.grip);
                ws = wv;
            }
            const float minLength = std::max(w.settings.suspensionMin, 0.0f);
            const float maxLength = std::max(w.settings.suspensionMax, minLength);
            const JPH::Vec3 direction = SuspensionDirection(w.settings);
            ws->mSuspensionDirection = direction;
            ws->mSteeringAxis = -direction;
            ws->mPosition = ToJolt(w.position) - direction * maxLength; // the wheel hangs at its entity
            ws->mSuspensionMinLength = minLength;
            ws->mSuspensionMaxLength = maxLength;
            ws->mSuspensionSpring.mFrequency = std::max(w.settings.suspensionFrequency, 0.01f);
            ws->mSuspensionSpring.mDamping = std::max(w.settings.suspensionDamping, 0.0f);
            ws->mRadius = std::max(w.settings.radius, 0.01f);
            ws->mWidth = std::max(w.settings.width, 0.01f);
            settings.mWheels.push_back(ws);

            // The wheel entity's own axes that lie along the axle (Jolt's right, -X) and up at rest.
            VehicleRecord::Wheel wheel;
            wheel.entity = w.entity.RawId();
            const glm::quat toModel = glm::inverse(w.rotation);
            wheel.modelRight = ToJolt(toModel * glm::vec3(-1.0f, 0.0f, 0.0f));
            wheel.modelUp = ToJolt(toModel * glm::vec3(0.0f, 1.0f, 0.0f));
            record.wheels.push_back(wheel);
        }

        const std::vector<std::vector<int>> axles = Axles(wheels);
        const float limitedSlip = vehicle->limitedSlipRatio > 1.0f ? vehicle->limitedSlipRatio : FLT_MAX;
        if (tracked)
        {
            auto* controller = new JPH::TrackedVehicleControllerSettings();
            SetupDrivetrain(*controller, *vehicle);
            for (int side = 0; side < 2; ++side)
            {
                // Jolt's left track is on +X.
                JPH::VehicleTrackSettings& track = controller->mTracks[side];
                int driven = -1;
                for (int i = 0; i < static_cast<int>(wheels.size()); ++i)
                {
                    if ((wheels[i].position.x > 0.0f) != (side == 0))
                        continue;
                    track.mWheels.push_back(static_cast<JPH::uint>(i));
                    if (driven < 0 || wheels[i].position.z < wheels[driven].position.z)
                        driven = i; // the rearmost wheel drives the track
                }
                if (driven < 0)
                {
                    Logger::Warning(std::string("Physics: ") + chassis.GetName() + " is tracked but has no wheels on one side");
                    return;
                }
                track.mDrivenWheel = static_cast<JPH::uint>(driven);
                track.mDifferentialRatio = vehicle->differentialRatio;
            }
            settings.mController = controller;
        }
        else
        {
            const bool motorcycle = vehicle->type == VehicleType::Motorcycle;
            JPH::WheeledVehicleControllerSettings* controller;
            if (motorcycle)
            {
                auto* bike = new JPH::MotorcycleControllerSettings();
                bike->mMaxLeanAngle = glm::radians(std::clamp(vehicle->maxLeanAngle, 0.0f, 80.0f));
                controller = bike;
            }
            else
            {
                controller = new JPH::WheeledVehicleControllerSettings();
            }
            SetupDrivetrain(*controller, *vehicle);
            std::vector<const std::vector<int>*> driven;
            if (motorcycle || vehicle->drive == VehicleDrive::Rear)
                driven.push_back(&axles.back());
            else if (vehicle->drive == VehicleDrive::Front)
                driven.push_back(&axles.front());
            else
                for (const std::vector<int>& axle : axles)
                    driven.push_back(&axle);
            for (const std::vector<int>* axle : driven)
            {
                JPH::VehicleDifferentialSettings differential;
                differential.mLeftWheel = (*axle)[0];
                differential.mRightWheel = axle->size() > 1 ? (*axle)[1] : -1;
                if (axle->size() == 1)
                    std::swap(differential.mLeftWheel, differential.mRightWheel); // a lone wheel: drive it fully
                differential.mDifferentialRatio = vehicle->differentialRatio;
                differential.mLimitedSlipRatio = limitedSlip;
                differential.mEngineTorqueRatio = 1.0f / static_cast<float>(driven.size());
                controller->mDifferentials.push_back(differential);
            }
            controller->mDifferentialLimitedSlipRatio = limitedSlip;
            settings.mController = controller;
            if (!motorcycle && vehicle->antiRollStiffness > 0.0f)
            {
                for (const std::vector<int>& axle : axles)
                {
                    if (axle.size() != 2)
                        continue;
                    JPH::VehicleAntiRollBar bar;
                    bar.mLeftWheel = axle[0];
                    bar.mRightWheel = axle[1];
                    bar.mStiffness = vehicle->antiRollStiffness;
                    settings.mAntiRollBars.push_back(bar);
                }
            }
        }

        JPH::BodyLockWrite lock(system->GetBodyLockInterface(), record.body);
        if (!lock.Succeeded())
            return;
        record.constraint = new JPH::VehicleConstraint(lock.GetBody(), settings);
        const JPH::ObjectLayer layer = lock.GetBody().GetObjectLayer();
        float narrowest = FLT_MAX;
        for (const WheelPlacement& w : wheels)
            narrowest = std::min(narrowest, w.settings.width);
        JPH::Ref<JPH::VehicleCollisionTester> tester;
        switch (vehicle->collision)
        {
        case WheelCollision::Ray: tester = new JPH::VehicleCollisionTesterRay(layer); break;
        case WheelCollision::Sphere: tester = new JPH::VehicleCollisionTesterCastSphere(layer, 0.5f * narrowest); break;
        case WheelCollision::Cylinder:
            // A rounded tyre on a motorcycle, a flat one otherwise.
            tester = new JPH::VehicleCollisionTesterCastCylinder(layer, vehicle->type == VehicleType::Motorcycle ? 1.0f : 0.1f);
            break;
        }
        record.wheelFilter = std::make_unique<WheelBodyFilter>(record.body);
        tester->SetBodyFilter(record.wheelFilter.get());
        record.constraint->SetVehicleCollisionTester(tester);
        if (vehicle->type == VehicleType::Wheeled)
        {
            // Jolt's own car was tuned with tyres that grip ten times harder along their rolling
            // direction (an old bug it kept the feel of); without that a car spins its wheels and its
            // gearbox, which won't shift while they slip, stays in first.
            static_cast<JPH::WheeledVehicleController*>(record.constraint->GetController())->SetTireMaxImpulseCallback(
                [](JPH::uint, float& longitudinal, float& lateral, float suspensionImpulse, float longitudinalFriction, float lateralFriction, float, float, float) {
                    longitudinal = 10.0f * longitudinalFriction * suspensionImpulse;
                    lateral = lateralFriction * suspensionImpulse;
                });
        }
        lock.ReleaseLock();
        system->AddConstraint(record.constraint);
        system->AddStepListener(record.constraint);
        // The wheel system shows each wheel entity where Jolt has its wheel.
        for (size_t i = 0; i < record.wheels.size(); ++i)
            if (Entity wheel = scene.FromId(record.wheels[i].entity); wheel.IsValid())
                wheel.Add<PhysicsWheel>(PhysicsWheel{ chassis.RawId(), static_cast<uint32_t>(i) });
    };
    for (Entity chassis : Find<Vehicle>())
        sync(chassis);

    for (auto it = vehicles.begin(); it != vehicles.end();)
    {
        if (it->second.seen)
        {
            ++it;
            continue;
        }
        RemoveVehicle(it->second);
        it = vehicles.erase(it);
    }
}

// Before each step: the drivers' inputs.
void PhysicsWorld::Impl::DriveVehicles()
{
    JPH::BodyInterface& bodies = system->GetBodyInterface();
    for (auto& [id, record] : vehicles)
    {
        const Entity entity = scene.FromId(id);
        const Vehicle* v = entity.IsValid() ? entity.TryGet<Vehicle>() : nullptr;
        if (!v || !record.constraint)
            continue;
        float forward = std::clamp(v->throttle, -1.0f, 1.0f);
        const float steer = std::clamp(v->steering, -1.0f, 1.0f);
        float brake = std::clamp(v->brake, 0.0f, 1.0f);
        const float handBrake = std::clamp(v->handBrake, 0.0f, 1.0f);
        if (forward != 0.0f || steer != 0.0f || brake != 0.0f || handBrake != 0.0f)
            bodies.ActivateBody(record.body);

        if (record.type == VehicleType::Tracked)
        {
            // Turning right (+X) slows the right track, which is Jolt's left one; at a standstill
            // it turns on the spot with the tracks running opposite ways.
            auto* controller = static_cast<JPH::TrackedVehicleController*>(record.constraint->GetController());
            const JPH::Vec3 velocity = bodies.GetRotation(record.body).Conjugated() * bodies.GetLinearVelocity(record.body);
            float left = 1.0f, right = 1.0f;
            if (steer != 0.0f && forward == 0.0f && brake == 0.0f && std::abs(velocity.GetZ()) < 1.0f)
            {
                forward = std::abs(steer);
                (steer > 0.0f ? left : right) = -1.0f;
            }
            else if (steer > 0.0f)
                left = 1.0f - 0.4f * steer;
            else if (steer < 0.0f)
                right = 1.0f + 0.4f * steer;
            controller->SetDriverInput(forward, left, right, std::max(brake, handBrake));
            if (!v->automatic)
                controller->GetTransmission().Set(v->gear, 1.0f);
        }
        else
        {
            auto* controller = static_cast<JPH::WheeledVehicleController*>(record.constraint->GetController());
            controller->SetDriverInput(forward, -steer, brake, handBrake); // Jolt's right is -X
            if (!v->automatic)
                controller->GetTransmission().Set(v->gear, 1.0f);
        }
    }
}

// After the steps: each wheel entity where its wheel is now (in its parent's space).
void PhysicsWorld::Impl::ShowWheel(Entity entity, const PhysicsWheel& wheel, LocalTRS& trs)
{
    // On the chassis as it is shown (interpolated): the write-back has just put it there.
    const auto vehicle = vehicles.find(wheel.chassis);
    const Entity chassis = scene.FromId(wheel.chassis);
    const PhysicsBody* shown = chassis.IsValid() ? chassis.TryGet<PhysicsBody>() : nullptr;
    if (vehicle == vehicles.end() || !vehicle->second.constraint || !shown || wheel.index >= vehicle->second.wheels.size())
        return;
    WheelRest& rest = wheelRests[entity.RawId()];
    if (trs.version != rest.writtenVersion && rest.writtenVersion != ~0u)
        return; // moved by someone else since: the next sync makes that its rest pose
    const VehicleRecord::Wheel& axes = vehicle->second.wheels[wheel.index];
    const JPH::Mat44 local = vehicle->second.constraint->GetWheelLocalTransform(wheel.index, axes.modelRight, axes.modelUp);
    const glm::vec3 position = shown->shownPosition + shown->shownRotation * ToGlm(local.GetTranslation());
    const glm::quat rotation = glm::normalize(shown->shownRotation * ToGlm(local.GetQuaternion()));
    SetWorldPose(scene, entity, trs, position, rotation);
    rest.writtenVersion = trs.version;
}

bool PhysicsWorld::GetVehicleInfo(Entity entity, VehicleInfo& info) const
{
    const auto found = m_Impl->vehicles.find(entity.RawId());
    if (found == m_Impl->vehicles.end() || !found->second.constraint)
        return false;
    const Impl::VehicleRecord& record = found->second;
    const JPH::BodyInterface& bodies = m_Impl->system->GetBodyInterface();
    info.speed = (bodies.GetRotation(record.body).Conjugated() * bodies.GetLinearVelocity(record.body)).GetZ();
    const JPH::VehicleController* controller = record.constraint->GetController();
    if (record.type == VehicleType::Tracked)
    {
        const auto* tracked = static_cast<const JPH::TrackedVehicleController*>(controller);
        info.rpm = tracked->GetEngine().GetCurrentRPM();
        info.gear = tracked->GetTransmission().GetCurrentGear();
    }
    else
    {
        const auto* wheeled = static_cast<const JPH::WheeledVehicleController*>(controller);
        info.rpm = wheeled->GetEngine().GetCurrentRPM();
        info.gear = wheeled->GetTransmission().GetCurrentGear();
    }
    info.wheels.clear();
    for (const JPH::Wheel* wheel : record.constraint->GetWheels())
    {
        const JPH::WheelSettings* s = wheel->GetSettings();
        VehicleInfo::Wheel w;
        w.contact = wheel->HasContact();
        const float travel = s->mSuspensionMaxLength - s->mSuspensionMinLength;
        w.suspension = travel > 1e-6f ? std::clamp((s->mSuspensionMaxLength - wheel->GetSuspensionLength()) / travel, 0.0f, 1.0f) : 0.0f;
        w.steerAngle = glm::degrees(wheel->GetSteerAngle());
        w.rpm = wheel->GetAngularVelocity() * 60.0f / glm::two_pi<float>();
        if (record.type != VehicleType::Tracked)
        {
            const auto* wv = static_cast<const JPH::WheelWV*>(wheel);
            w.longitudinalSlip = wv->mLongitudinalSlip;
            w.lateralSlip = glm::degrees(wv->mLateralSlip);
        }
        info.wheels.push_back(w);
    }
    return true;
}
