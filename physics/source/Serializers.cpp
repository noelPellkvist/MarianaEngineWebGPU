// The physics components in scene files (SceneFile), registered with them by
// PhysicsWorld::RegisterComponents. What drives them while the game runs (a vehicle's pedals, a
// character's walking, a broken joint) isn't saved.
#include <Mariana/Physics/PhysicsWorld.hpp>
#include <Mariana/Physics/Controllers.hpp>
#include <Mariana/Importers/SceneFile.hpp>
#include <Mariana/Graphics/Mesh.hpp>

namespace
{
    void ColliderFields(SceneArchive& a, Collider& c)
    {
        a.Field("center", c.center);
        a.Field("rotation", c.rotation);
        a.Field("friction", c.friction);
        a.Field("restitution", c.restitution);
        a.Field("density", c.density);
        a.Field("trigger", c.isTrigger);
    }
}

namespace PhysicsInternal
{
    void RegisterSerializers()
    {
        SceneFile::RegisterComponent<RigidBody>("RigidBody", [](SceneArchive& a, RigidBody& b) {
            a.Field("motion", b.motion, { "Static", "Kinematic", "Dynamic" });
            a.Field("mass", b.mass);
            a.Field("linearDamping", b.linearDamping);
            a.Field("angularDamping", b.angularDamping);
            a.Field("gravityFactor", b.gravityFactor);
            a.Field("continuous", b.continuous);
            a.Field("allowSleeping", b.allowSleeping);
            a.Field("lockedAxes", b.lockedAxes);
            a.Field("layer", b.layer);
            a.Field("startVelocity", b.startVelocity);
            a.Field("startAngularVelocity", b.startAngularVelocity);
            a.Field("centerOfMass", b.centerOfMass);
            a.Field("interpolate", b.interpolate);
            a.Field("buoyancy", b.buoyancy);
        });
        SceneFile::RegisterComponent<BoxCollider>("BoxCollider", [](SceneArchive& a, BoxCollider& c) {
            ColliderFields(a, c);
            a.Field("size", c.size);
        });
        SceneFile::RegisterComponent<SphereCollider>("SphereCollider", [](SceneArchive& a, SphereCollider& c) {
            ColliderFields(a, c);
            a.Field("radius", c.radius);
        });
        SceneFile::RegisterComponent<CapsuleCollider>("CapsuleCollider", [](SceneArchive& a, CapsuleCollider& c) {
            ColliderFields(a, c);
            a.Field("radius", c.radius);
            a.Field("height", c.height);
        });
        SceneFile::RegisterComponent<CylinderCollider>("CylinderCollider", [](SceneArchive& a, CylinderCollider& c) {
            ColliderFields(a, c);
            a.Field("radius", c.radius);
            a.Field("height", c.height);
        });
        SceneFile::RegisterComponent<MeshCollider>("MeshCollider", [](SceneArchive& a, MeshCollider& c) {
            ColliderFields(a, c);
            a.Asset("mesh", c.mesh);
            a.Field("convex", c.convex);
        });
        SceneFile::RegisterComponent<Vehicle>("Vehicle", [](SceneArchive& a, Vehicle& v) {
            a.Field("type", v.type, { "Wheeled", "Motorcycle", "Tracked" });
            a.Field("collision", v.collision, { "Ray", "Sphere", "Cylinder" });
            a.Field("drive", v.drive, { "Front", "Rear", "All" });
            a.Field("engineTorque", v.engineTorque);
            a.Field("minRPM", v.minRPM);
            a.Field("maxRPM", v.maxRPM);
            a.Field("automatic", v.automatic);
            a.Field("shiftUpRPM", v.shiftUpRPM);
            a.Field("shiftDownRPM", v.shiftDownRPM);
            a.Field("clutchStrength", v.clutchStrength);
            a.Field("gearRatios", v.gearRatios);
            a.Field("reverseRatio", v.reverseRatio);
            a.Field("differentialRatio", v.differentialRatio);
            a.Field("limitedSlipRatio", v.limitedSlipRatio);
            a.Field("antiRollStiffness", v.antiRollStiffness);
            a.Field("maxPitchRollAngle", v.maxPitchRollAngle);
            a.Field("maxLeanAngle", v.maxLeanAngle);
        });
        SceneFile::RegisterComponent<VehicleWheel>("VehicleWheel", [](SceneArchive& a, VehicleWheel& w) {
            a.Field("radius", w.radius);
            a.Field("width", w.width);
            a.Field("suspensionMin", w.suspensionMin);
            a.Field("suspensionMax", w.suspensionMax);
            a.Field("suspensionFrequency", w.suspensionFrequency);
            a.Field("suspensionDamping", w.suspensionDamping);
            a.Field("maxSteerAngle", w.maxSteerAngle);
            a.Field("casterAngle", w.casterAngle);
            a.Field("maxBrakeTorque", w.maxBrakeTorque);
            a.Field("maxHandBrakeTorque", w.maxHandBrakeTorque);
            a.Field("grip", w.grip);
        });
        SceneFile::RegisterComponent<PlayerControl>("PlayerControl", [](SceneArchive& a, PlayerControl& c) {
            a.Field("enabled", c.enabled);
            a.Field("steerSpeed", c.steerSpeed);
            a.Field("steerFade", c.steerFade);
        });
        SceneFile::RegisterComponent<FollowCamera>("FollowCamera", [](SceneArchive& a, FollowCamera& f) {
            a.Field("target", f.target);
            a.Field("distance", f.distance);
            a.Field("height", f.height);
            a.Field("lookHeight", f.lookHeight);
            a.Field("stiffness", f.stiffness);
        });
        SceneFile::RegisterComponent<CharacterController>("CharacterController", [](SceneArchive& a, CharacterController& c) {
            a.Field("radius", c.radius);
            a.Field("height", c.height);
            a.Field("maxSlope", c.maxSlope);
            a.Field("stepHeight", c.stepHeight);
            a.Field("stickDown", c.stickDown);
            a.Field("mass", c.mass);
            a.Field("strength", c.strength);
            a.Field("speed", c.speed);
            a.Field("runFactor", c.runFactor);
            a.Field("jumpSpeed", c.jumpSpeed);
            a.Field("gravityFactor", c.gravityFactor);
            a.Field("airControl", c.airControl);
            a.Field("turnToMove", c.turnToMove);
            a.Field("turnSpeed", c.turnSpeed);
            a.Field("layer", c.layer);
        });
        SceneFile::RegisterComponent<WaterVolume>("WaterVolume", [](SceneArchive& a, WaterVolume& w) {
            a.Field("size", w.size);
            a.Field("density", w.density);
            a.Field("linearDrag", w.linearDrag);
            a.Field("angularDrag", w.angularDrag);
            a.Field("damping", w.damping);
            a.Field("flow", w.flow);
            a.Field("waveHeight", w.waveHeight);
            a.Field("waveLength", w.waveLength);
            a.Field("waveSpeed", w.waveSpeed);
            a.Field("rippleSpeed", w.rippleSpeed);
        });
        SceneFile::RegisterComponent<Ragdoll>("Ragdoll", [](SceneArchive& a, Ragdoll& r) {
            a.Field("mode", r.mode, { "Animated", "Powered", "Limp" });
            a.Field("mass", r.mass);
            a.Field("strength", r.strength);
            a.Field("limpOnImpact", r.limpOnImpact);
            a.Field("friction", r.friction);
            a.Field("jointLimitScale", r.jointLimitScale);
            a.Field("layer", r.layer);
        });
        SceneFile::RegisterComponent<Joint>("Joint", [](SceneArchive& a, Joint& j) {
            a.Field("type", j.type, { "Fixed", "Point", "Distance", "Hinge", "Slider", "Cone", "SwingTwist", "SixDOF" });
            a.Field("connected", j.connected);
            a.Field("toParent", j.toParent);
            a.Field("collideConnected", j.collideConnected);
            a.Field("limited", j.limited);
            a.Field("minLimit", j.minLimit);
            a.Field("maxLimit", j.maxLimit);
            a.Field("connectedAnchor", j.connectedAnchor);
            a.Field("minDistance", j.minDistance);
            a.Field("maxDistance", j.maxDistance);
            a.Field("springFrequency", j.springFrequency);
            a.Field("springDamping", j.springDamping);
            a.Field("coneAngle", j.coneAngle);
            a.Field("twistMin", j.twistMin);
            a.Field("twistMax", j.twistMax);
            a.Field("lockedAxes", j.lockedAxes);
            a.Field("motor", j.motor, { "Off", "Velocity", "Position" });
            a.Field("motorSpeed", j.motorSpeed);
            a.Field("motorTarget", j.motorTarget);
            a.Field("motorStrength", j.motorStrength);
            a.Field("motorFrequency", j.motorFrequency);
            a.Field("friction", j.friction);
            a.Field("breakForce", j.breakForce);
            a.Field("breakTorque", j.breakTorque);
        });
    }
}
