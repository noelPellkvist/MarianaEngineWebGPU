#include "WorldImpl.hpp"

#include <Mariana/Animation/Animator.hpp>
#include <Mariana/Animation/Skeleton.hpp>
#include <Mariana/Animation/Skin.hpp>
#include <Mariana/Foundation/Logger.hpp>
#include <Mariana/Graphics/Mesh.hpp>
#include <Mariana/Rendering/Components.hpp>

#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/GroupFilterTable.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>
#include <Jolt/Skeleton/SkeletonPose.h>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <unordered_map>
#include <unordered_set>

using namespace PhysicsInternal;

namespace
{
    // A bone's name in lower case, without a "namespace:" (Mixamo's "mixamorig:LeftArm").
    std::string BaseName(const char* name)
    {
        std::string text = name ? name : "";
        if (const size_t colon = text.rfind(':'); colon != std::string::npos)
            text = text.substr(colon + 1);
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return text;
    }

    bool Has(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }

    // What a bone is by its name; Other with `skip` for what a ragdoll leaves out.
    RagdollJointKind KindOf(const std::string& name, bool& skip)
    {
        skip = false;
        for (const char* left : { "thumb", "index", "middle", "ring", "pinky", "finger", "toe", "end", "eye", "jaw", "tongue",
                                  "twist", "clavicle", "shoulder", "breast", "hair", "armature", "nub" })
        {
            if (Has(name, left))
            {
                skip = true;
                return RagdollJointKind::Other;
            }
        }
        if (Has(name, "hips") || Has(name, "pelvis"))
            return RagdollJointKind::Root;
        if (Has(name, "spine") || Has(name, "chest"))
            return RagdollJointKind::Spine;
        if (Has(name, "neck"))
            return RagdollJointKind::Neck;
        if (Has(name, "head"))
            return RagdollJointKind::Head;
        if (Has(name, "forearm") || Has(name, "lowerarm") || Has(name, "elbow"))
            return RagdollJointKind::Elbow;
        if (Has(name, "hand") || Has(name, "wrist"))
            return RagdollJointKind::Wrist;
        if (Has(name, "upleg") || Has(name, "thigh") || Has(name, "upperleg"))
            return RagdollJointKind::Hip;
        if (Has(name, "foot") || Has(name, "ankle"))
            return RagdollJointKind::Ankle;
        if (Has(name, "leg") || Has(name, "calf") || Has(name, "shin") || Has(name, "knee"))
            return RagdollJointKind::Knee;
        if (Has(name, "upperarm") || (name.size() >= 3 && name.compare(name.size() - 3, 3, "arm") == 0))
            return RagdollJointKind::Shoulder;
        return RagdollJointKind::Other;
    }

    glm::quat RotationOf(const glm::mat4& m)
    {
        glm::vec3 position;
        glm::quat rotation;
        SplitPose(m, position, rotation);
        return rotation;
    }

    void AddBox(Bounds& bounds, const Bounds& box, const glm::mat4& to)
    {
        for (int i = 0; i < 8; ++i)
            bounds.Add(glm::vec3(to * glm::vec4((i & 1) ? box.max.x : box.min.x, (i & 2) ? box.max.y : box.min.y, (i & 4) ? box.max.z : box.min.z, 1.0f)));
    }

    glm::mat4 TRS(const glm::vec3& t, const glm::quat& r, const glm::vec3& s)
    {
        glm::mat4 m = glm::mat4_cast(r);
        m[0] *= s.x;
        m[1] *= s.y;
        m[2] *= s.z;
        m[3] = glm::vec4(t, 1.0f);
        return m;
    }

    // The joint limits of a kind (degrees): swing cone (normal, plane), twist.
    struct Limits { float normal, plane, twist; };
    Limits LimitsOf(RagdollJointKind kind)
    {
        switch (kind)
        {
        case RagdollJointKind::Spine: return { 30.0f, 30.0f, 20.0f };
        case RagdollJointKind::Neck: return { 45.0f, 45.0f, 35.0f };
        case RagdollJointKind::Head: return { 50.0f, 50.0f, 45.0f };
        case RagdollJointKind::Shoulder: return { 100.0f, 100.0f, 70.0f };
        case RagdollJointKind::Wrist: return { 60.0f, 60.0f, 40.0f };
        case RagdollJointKind::Hip: return { 80.0f, 60.0f, 30.0f };
        case RagdollJointKind::Ankle: return { 45.0f, 45.0f, 20.0f };
        default: return { 45.0f, 45.0f, 30.0f };
        }
    }

    // The shortest turn taking unit vector `from` to unit vector `to`.
    glm::quat FromTo(const glm::vec3& from, const glm::vec3& to)
    {
        const float d = glm::dot(from, to);
        if (d < -0.9999f)
        {
            glm::vec3 axis = glm::cross(glm::vec3(1.0f, 0.0f, 0.0f), from);
            if (glm::length(axis) < 1e-3f)
                axis = glm::cross(glm::vec3(0.0f, 0.0f, 1.0f), from);
            return glm::angleAxis(glm::pi<float>(), glm::normalize(axis));
        }
        const glm::vec3 c = glm::cross(from, to);
        return glm::normalize(glm::quat(1.0f + d, c.x, c.y, c.z));
    }

    float CapsuleVolume(const RagdollBone& b)
    {
        const float length = glm::length(b.b - b.a);
        return glm::pi<float>() * b.radius * b.radius * length + 4.0f / 3.0f * glm::pi<float>() * b.radius * b.radius * b.radius;
    }
}

std::vector<RagdollBone> PlanRagdoll(const Scene& scene, Entity root)
{
    std::vector<RagdollBone> plan;
    if (!root.IsValid())
        return plan;

    // The entities below the root by their glTF node, and the skins' bind pose: each joint's frame
    // and the box around the vertices it moves, in world space.
    std::unordered_map<uint32_t, Entity> byNode;
    std::vector<Entity> below;
    scene.ForEachDescendant(root, [&](Entity e) {
        below.push_back(e);
        if (const AnimationTargetEntity* target = e.TryGet<AnimationTargetEntity>())
            byNode.emplace(target->nodeIndex, e);
    });
    std::unordered_map<uint64_t, glm::mat4> bind;
    std::unordered_map<uint64_t, Bounds> skinBounds;
    for (Entity e : below)
    {
        const SkinComponent* skinComponent = e.TryGet<SkinComponent>();
        const MeshComponent* meshComponent = e.TryGet<MeshComponent>();
        const Skin* skin = skinComponent ? skinComponent->skin.Get().get() : nullptr;
        if (!skin)
            continue;
        const glm::mat4 meshWorld = WorldMatrix(scene, e);
        const IMesh* mesh = meshComponent ? meshComponent->mesh.Get().get() : nullptr;
        for (size_t j = 0; j < skin->jointNodes.size() && j < skin->inverseBind.size(); ++j)
        {
            const auto found = byNode.find(skin->jointNodes[j]);
            if (found == byNode.end())
                continue;
            const uint64_t id = found->second.RawId();
            bind.emplace(id, meshWorld * glm::inverse(skin->inverseBind[j]));
            if (mesh && j < mesh->jointBounds.size() && !mesh->jointBounds[j].IsEmpty())
                AddBox(skinBounds[id], mesh->jointBounds[j], meshWorld);
        }
    }
    const bool skinned = !bind.empty();

    // The bones: by name, those with a body.
    std::unordered_map<uint64_t, int> index;
    std::function<void(Entity, int)> visit = [&](Entity e, int parent) {
        int here = parent;
        bool skip = false;
        const RagdollJointKind kind = KindOf(BaseName(e.GetName()), skip);
        const bool joint = !skinned || bind.count(e.RawId());
        if (!skip && joint && (kind != RagdollJointKind::Other || (skinned && skinBounds.count(e.RawId()))))
        {
            RagdollBone bone;
            bone.entity = e;
            bone.name = e.GetName() ? e.GetName() : "";
            bone.parent = parent;
            bone.joint = parent < 0 ? RagdollJointKind::Root : kind;
            const glm::mat4 frame = skinned ? bind[e.RawId()] : WorldMatrix(scene, e);
            bone.position = glm::vec3(frame[3]);
            bone.rotation = RotationOf(frame);
            here = static_cast<int>(plan.size());
            index[e.RawId()] = here;
            plan.push_back(bone);
        }
        e.ForEachChild([&](Entity child) { visit(child, here); });
    };
    root.ForEachChild([&](Entity child) { visit(child, -1); });
    if (plan.empty())
        return plan;

    // Bones with nothing of a body in them (an "Other" with a tiny box) aren't worth one.
    float height = 0.0f;
    {
        Bounds all;
        for (const RagdollBone& b : plan)
            all.Add(b.position);
        height = std::max(all.max.y - all.min.y, 0.1f);
    }

    // Where a bone's joint is for the frames of the bones: the position of an entity in bind pose.
    auto positionOf = [&](Entity e) {
        const auto found = bind.find(e.RawId());
        return found != bind.end() ? glm::vec3(found->second[3]) : glm::vec3(WorldMatrix(scene, e)[3]);
    };

    for (int i = 0; i < static_cast<int>(plan.size()); ++i)
    {
        RagdollBone& bone = plan[i];
        bone.a = bone.position;
        // The capsule runs to the next bone of its chain with a body, else to its children (fingers,
        // toes, the head's top), else through its skin's box.
        std::vector<int> bodyChildren;
        for (int c = i + 1; c < static_cast<int>(plan.size()); ++c)
            if (plan[c].parent == i)
                bodyChildren.push_back(c);
        std::vector<glm::vec3> childPositions;
        bone.entity.ForEachChild([&](Entity child) { childPositions.push_back(positionOf(child)); });

        if (bone.joint == RagdollJointKind::Root)
        {
            // Hips: across, between where the thighs start.
            std::vector<glm::vec3> thighs;
            for (int c : bodyChildren)
                if (plan[c].joint == RagdollJointKind::Hip)
                    thighs.push_back(plan[c].position);
            if (thighs.size() == 2)
            {
                bone.a = thighs[0];
                bone.b = thighs[1];
            }
            else
            {
                bone.b = bone.a + glm::vec3(0.0f, 0.1f * height, 0.0f);
            }
        }
        else
        {
            const RagdollJointKind next = bone.joint == RagdollJointKind::Spine ? RagdollJointKind::Spine
                                        : bone.joint == RagdollJointKind::Neck ? RagdollJointKind::Head
                                        : bone.joint == RagdollJointKind::Shoulder ? RagdollJointKind::Elbow
                                        : bone.joint == RagdollJointKind::Elbow ? RagdollJointKind::Wrist
                                        : bone.joint == RagdollJointKind::Hip ? RagdollJointKind::Knee
                                        : bone.joint == RagdollJointKind::Knee ? RagdollJointKind::Ankle : RagdollJointKind::Other;
            int toward = -1;
            for (int c : bodyChildren)
                if (plan[c].joint == next || (bone.joint == RagdollJointKind::Spine && plan[c].joint == RagdollJointKind::Neck))
                    toward = toward < 0 ? c : toward;
            if (toward >= 0)
                bone.b = plan[toward].position;
            else if (!childPositions.empty())
            {
                glm::vec3 sum(0.0f);
                for (const glm::vec3& p : childPositions)
                    sum += p;
                bone.b = sum / static_cast<float>(childPositions.size());
            }
            else if (const auto box = skinBounds.find(bone.entity.RawId()); box != skinBounds.end())
                bone.b = 0.5f * (box->second.min + box->second.max);
            else
                bone.b = bone.a + bone.rotation * glm::vec3(0.0f, 0.1f * height, 0.0f);
        }
        if (glm::length(bone.b - bone.a) < 1e-3f)
            bone.b = bone.a + glm::vec3(0.0f, 0.02f * height, 0.0f);

        // Thick as the skin around it (its box seen across the capsule's axis), a little less as the
        // box also holds what the neighbours share.
        const glm::vec3 axis = glm::normalize(bone.b - bone.a);
        const float length = glm::length(bone.b - bone.a);
        if (const auto box = skinBounds.find(bone.entity.RawId()); box != skinBounds.end())
        {
            const glm::vec3 half = 0.5f * (box->second.max - box->second.min);
            const glm::vec3 across = glm::vec3(1.0f) - glm::abs(axis);
            bone.radius = 0.8f * glm::dot(half, across) / std::max(across.x + across.y + across.z, 1e-3f);
        }
        else
        {
            bone.radius = 0.2f * length;
        }
        bone.radius = std::clamp(bone.radius, 0.02f * height, 0.12f * height);
        if (bone.joint == RagdollJointKind::Root && glm::length(bone.b - bone.a) > 2.0f * bone.radius)
        {
            // Not into the thighs' capsules.
            const glm::vec3 middle = 0.5f * (bone.a + bone.b);
            bone.a = middle + (bone.a - middle) * 0.5f;
            bone.b = middle + (bone.b - middle) * 0.5f;
        }
    }
    return plan;
}

// One ragdoll of the world: a Jolt ragdoll for a model's root, its bones' entities, how it moves
// now, and its bones' poses for interpolation.
void PhysicsWorld::Impl::SyncRagdolls()
{
    for (auto& [id, record] : ragdolls)
        record.seen = false;

    for (Entity root : Find({ &ragdollFinder }))
    {
        const Ragdoll& settings = *root.TryGet<Ragdoll>();
        uint64_t signature = 1469598103934665603ull;
        auto mix = [&](const void* data, size_t size) {
            for (size_t i = 0; i < size; ++i)
                signature = (signature ^ static_cast<const unsigned char*>(data)[i]) * 1099511628211ull;
        };
        mix(&settings.mass, sizeof(float));
        mix(&settings.friction, sizeof(float));
        mix(&settings.jointLimitScale, sizeof(float));
        mix(&settings.layer, sizeof(uint32_t));

        RagdollRecord& record = ragdolls[root.RawId()];
        record.seen = true;
        if (record.ragdoll && record.signature == signature)
            continue;
        RemoveRagdoll(record);
        record = RagdollRecord();
        record.seen = true;
        record.signature = signature;
        BuildRagdoll(root, settings, record);
    }

    for (auto it = ragdolls.begin(); it != ragdolls.end();)
    {
        if (it->second.seen)
        {
            ++it;
            continue;
        }
        RemoveRagdoll(it->second);
        it = ragdolls.erase(it);
    }
}

void PhysicsWorld::Impl::BuildRagdoll(Entity root, const Ragdoll& settings, RagdollRecord& record)
{
    const std::vector<RagdollBone> plan = PlanRagdoll(scene, root);
    if (plan.size() < 2)
    {
        if (warnedDynamicMesh.insert(root.RawId()).second)
            Logger::Warning(std::string("Physics: ") + root.GetName() + " has a Ragdoll but no bones to make one of");
        return;
    }
    record.root = root.RawId();
    const glm::mat4 rootWorld = WorldMatrix(scene, root);
    glm::vec3 forward = glm::vec3(rootWorld[2]);
    forward.y = 0.0f;
    forward = glm::length(forward) > 1e-4f ? glm::normalize(forward) : glm::vec3(0.0f, 0.0f, 1.0f);

    float totalVolume = 0.0f;
    for (const RagdollBone& b : plan)
        totalVolume += CapsuleVolume(b);

    JPH::Ref<JPH::Skeleton> skeleton = new JPH::Skeleton();
    for (const RagdollBone& b : plan)
        skeleton->AddJoint(b.name, b.parent);
    JPH::Ref<JPH::RagdollSettings> ragdollSettings = new JPH::RagdollSettings();
    ragdollSettings->mSkeleton = skeleton;
    ragdollSettings->mParts.resize(plan.size());
    std::vector<JPH::Mat44> bindMatrices(plan.size());
    const float scale = std::max(settings.jointLimitScale, 0.0f);

    for (size_t i = 0; i < plan.size(); ++i)
    {
        const RagdollBone& bone = plan[i];
        JPH::RagdollSettings::Part& part = ragdollSettings->mParts[i];
        // The capsule in the bone's frame: centred between its ends, turned from +Y to them.
        const glm::quat inverse = glm::inverse(bone.rotation);
        const glm::vec3 a = inverse * (bone.a - bone.position), b = inverse * (bone.b - bone.position);
        const float length = glm::length(b - a);
        const glm::vec3 direction = length > 1e-5f ? (b - a) / length : glm::vec3(0.0f, 1.0f, 0.0f);
        const glm::quat turn = FromTo(glm::vec3(0.0f, 1.0f, 0.0f), direction);
        const float halfHeight = std::max(0.5f * length * 0.85f, 0.005f);
        JPH::RotatedTranslatedShapeSettings shape(ToJolt(0.5f * (a + b)), ToJolt(glm::normalize(turn)), new JPH::CapsuleShapeSettings(halfHeight, bone.radius));
        const JPH::ShapeSettings::ShapeResult made = shape.Create();
        if (made.HasError())
            return;
        part.SetShape(made.Get());
        part.mPosition = JPH::RVec3(ToJolt(bone.position));
        part.mRotation = ToJolt(bone.rotation);
        part.mMotionType = JPH::EMotionType::Dynamic;
        part.mObjectLayer = MakeLayer(settings.layer, true);
        part.mFriction = std::max(settings.friction, 0.0f);
        part.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        part.mMassPropertiesOverride.mMass = std::max(settings.mass, 1.0f) * CapsuleVolume(bone) / std::max(totalVolume, 1e-6f);
        part.mLinearDamping = 0.1f;
        part.mAngularDamping = 0.3f;
        // A chain of joints from the hips to the head and hands: more solver passes than Jolt's
        // default so the motors' pose holds along it.
        part.mNumVelocityStepsOverride = 30;
        part.mNumPositionStepsOverride = 6;
        bindMatrices[i] = JPH::Mat44::sRotationTranslation(ToJolt(bone.rotation), ToJolt(bone.position));

        if (bone.parent < 0)
            continue;
        // The joint at this bone's origin, along the way the bone points.
        const glm::vec3 twist = glm::normalize(bone.b - bone.a);
        if (bone.joint == RagdollJointKind::Knee || bone.joint == RagdollJointKind::Elbow)
        {
            // A hinge bending the way the limb does: knees back, elbows forward.
            const glm::vec3 flex = bone.joint == RagdollJointKind::Knee ? -forward : forward;
            glm::vec3 axis = glm::cross(twist, flex);
            if (glm::length(axis) < 1e-3f)
                axis = glm::cross(twist, glm::vec3(0.0f, 1.0f, 0.0f));
            axis = glm::normalize(axis);
            JPH::Ref<JPH::HingeConstraintSettings> hinge = new JPH::HingeConstraintSettings();
            hinge->mSpace = JPH::EConstraintSpace::WorldSpace;
            hinge->mPoint1 = hinge->mPoint2 = JPH::RVec3(ToJolt(bone.position));
            hinge->mHingeAxis1 = hinge->mHingeAxis2 = ToJolt(axis);
            hinge->mNormalAxis1 = hinge->mNormalAxis2 = ToJolt(twist);
            hinge->mLimitsMin = glm::radians(-5.0f * scale);
            hinge->mLimitsMax = glm::radians(std::min((bone.joint == RagdollJointKind::Knee ? 140.0f : 145.0f) * scale, 179.0f));
            hinge->mMaxFrictionTorque = 2.0f;
            part.mToParent = hinge;
        }
        else
        {
            const Limits limits = LimitsOf(bone.joint);
            glm::vec3 plane = forward - twist * glm::dot(forward, twist);
            if (glm::length(plane) < 1e-3f)
                plane = bone.rotation * glm::vec3(1.0f, 0.0f, 0.0f) - twist * glm::dot(bone.rotation * glm::vec3(1.0f, 0.0f, 0.0f), twist);
            plane = glm::normalize(plane);
            JPH::Ref<JPH::SwingTwistConstraintSettings> swing = new JPH::SwingTwistConstraintSettings();
            swing->mSpace = JPH::EConstraintSpace::WorldSpace;
            swing->mPosition1 = swing->mPosition2 = JPH::RVec3(ToJolt(bone.position));
            swing->mTwistAxis1 = swing->mTwistAxis2 = ToJolt(twist);
            swing->mPlaneAxis1 = swing->mPlaneAxis2 = ToJolt(plane);
            swing->mNormalHalfConeAngle = glm::radians(std::min(limits.normal * scale, 179.0f));
            swing->mPlaneHalfConeAngle = glm::radians(std::min(limits.plane * scale, 179.0f));
            swing->mTwistMinAngle = glm::radians(-std::min(limits.twist * scale, 179.0f));
            swing->mTwistMaxAngle = glm::radians(std::min(limits.twist * scale, 179.0f));
            swing->mMaxFrictionTorque = 2.0f;
            part.mToParent = swing;
        }
    }

    ragdollSettings->Stabilize();
    ragdollSettings->DisableParentChildCollisions(bindMatrices.data(), 0.0f);
    // Its own bodies never collide with each other: an animated pose brings limbs close (hands to
    // the face, arms along the body), and capsules pushing apart would drag it off the pose.
    if (auto* table = dynamic_cast<JPH::GroupFilterTable*>(const_cast<JPH::GroupFilter*>(ragdollSettings->mParts[0].mCollisionGroup.GetGroupFilter())))
        for (JPH::CollisionGroup::SubGroupID a = 0; a < plan.size(); ++a)
            for (JPH::CollisionGroup::SubGroupID b = a + 1; b < plan.size(); ++b)
                table->DisableCollision(a, b);
    ragdollSettings->CalculateConstraintPriorities(); // solved from the hips outward
    ragdollSettings->CalculateBodyIndexToConstraintIndex();
    ragdollSettings->CalculateConstraintIndexToBodyIdxPair();
    record.settings = ragdollSettings;
    // Group ids well above the body indices the joints' filter uses.
    record.ragdoll = ragdollSettings->CreateRagdoll(static_cast<JPH::CollisionGroup::GroupID>(0x40000000u + ++ragdollGroups), root.RawId(), system.get());
    if (!record.ragdoll)
        return;
    record.ragdoll->AddToPhysicsSystem(JPH::EActivation::Activate);

    // Each body reports its bone's entity (contacts, queries).
    JPH::BodyInterface& bodies = system->GetBodyInterface();
    record.bones.resize(plan.size());
    for (size_t i = 0; i < plan.size(); ++i)
    {
        RagdollRecord::Bone& bone = record.bones[i];
        bone.entity = plan[i].entity.RawId();
        bone.body = record.ragdoll->GetBodyID(static_cast<int>(i));
        bone.radius = plan[i].radius;
        const glm::quat inverse = glm::inverse(plan[i].rotation);
        bone.localA = inverse * (plan[i].a - plan[i].position);
        bone.localB = inverse * (plan[i].b - plan[i].position);
        if (const LocalTRS* trs = plan[i].entity.TryGet<LocalTRS>())
            bone.rest = *trs;
        bodies.SetUserData(bone.body, bone.entity);
        bodyEntities[bone.body.GetIndexAndSequenceNumber()] = bone.entity;
        ragdollBones[bone.entity] = root.RawId();
    }

    // Where the model is now (the animation's pose), still.
    JPH::SkeletonPose pose;
    if (TargetPose(record, pose))
    {
        record.ragdoll->SetPose(pose);
        record.ragdoll->SetLinearAndAngularVelocity(JPH::Vec3::sZero(), JPH::Vec3::sZero());
    }
    record.mode = static_cast<RagdollMode>(255); // applied at the first drive
    record.asked = settings.mode;

    // The write-back shows each bone's entity where its body is (from where it is now).
    for (const RagdollRecord::Bone& bone : record.bones)
    {
        JPH::RVec3 p;
        JPH::Quat q;
        bodies.GetPositionAndRotation(bone.body, p, q);
        PhysicsBody shown;
        shown.body = bone.body.GetIndexAndSequenceNumber();
        shown.Place(ToGlm(JPH::Vec3(p)), ToGlm(q));
        if (Entity e = scene.FromId(bone.entity); e.IsValid())
            e.Add<PhysicsBody>(shown);
    }
}

void PhysicsWorld::Impl::RemoveRagdoll(RagdollRecord& record)
{
    if (!record.ragdoll)
        return;
    record.ragdoll->RemoveFromPhysicsSystem();
    for (const RagdollRecord::Bone& bone : record.bones)
    {
        removedBodies.push_back(bone.body.GetIndexAndSequenceNumber());
        ragdollBones.erase(bone.entity);
        if (Entity e = scene.FromId(bone.entity); e.IsValid() && e.TryGet<PhysicsBody>())
            e.Remove<PhysicsBody>();
    }
    record.ragdoll = nullptr; // destroys its bodies
    // The animation takes the bones back.
    if (Animator* animator = scene.FromId(record.root).IsValid() ? scene.FromId(record.root).Get<Animator>() : nullptr)
        animator->Refresh();
}

bool PhysicsWorld::Impl::TargetPose(const RagdollRecord& record, JPH::SkeletonPose& pose)
{
    Entity root = scene.FromId(record.root);
    if (!root.IsValid() || !record.settings)
        return false;
    const Animator* animator = root.TryGet<Animator>();
    const Skeleton* skeleton = animator ? animator->skeleton.Get().get() : nullptr;
    const Pose* animated = animator && skeleton && animator->GetPose().BoneCount() == skeleton->BoneCount() ? &animator->GetPose() : nullptr;

    // Each entity's world matrix as the animation has it: the ragdoll's bones are written by the
    // simulation, so their local transform is the animation's (or, without one, the one they had).
    std::unordered_map<uint64_t, const RagdollRecord::Bone*> bones;
    for (const RagdollRecord::Bone& bone : record.bones)
        bones[bone.entity] = &bone;
    std::unordered_map<uint64_t, glm::mat4> memo;
    std::function<glm::mat4(Entity)> world = [&](Entity e) -> glm::mat4 {
        if (e.RawId() == record.root)
            return WorldMatrix(scene, e);
        if (const auto found = memo.find(e.RawId()); found != memo.end())
            return found->second;
        glm::mat4 local(1.0f);
        const auto bone = bones.find(e.RawId());
        int skeletonBone = -1;
        if (animated)
            if (const AnimationTargetEntity* target = e.TryGet<AnimationTargetEntity>())
                skeletonBone = skeleton->FindBoneByNode(target->nodeIndex);
        if (bone != bones.end() && skeletonBone >= 0)
            local = TRS(animated->translations[skeletonBone], animated->rotations[skeletonBone], animated->scales[skeletonBone]);
        else if (bone != bones.end())
            local = LocalMatrix(bone->second->rest);
        else if (const LocalTRS* trs = e.TryGet<LocalTRS>())
            local = LocalMatrix(*trs);
        const Entity parent = scene.Parent(e);
        const glm::mat4 m = (parent.IsValid() ? world(parent) : glm::mat4(1.0f)) * local;
        memo[e.RawId()] = m;
        return m;
    };

    pose.SetSkeleton(record.settings->GetSkeleton());
    pose.SetRootOffset(JPH::RVec3::sZero());
    for (size_t i = 0; i < record.bones.size(); ++i)
    {
        glm::vec3 position;
        glm::quat rotation;
        SplitPose(world(scene.FromId(record.bones[i].entity)), position, rotation);
        pose.GetJointMatrix(static_cast<int>(i)) = JPH::Mat44::sRotationTranslation(ToJolt(rotation), ToJolt(position));
    }
    pose.CalculateJointStates();
    return true;
}

void PhysicsWorld::Impl::DriveRagdolls(float dt)
{
    JPH::BodyInterface& bodies = system->GetBodyInterface();
    for (auto& [id, record] : ragdolls)
    {
        Entity root = scene.FromId(id);
        const Ragdoll* settings = root.IsValid() ? root.TryGet<Ragdoll>() : nullptr;
        if (!record.ragdoll || !settings)
            continue;
        if (settings->mode != record.asked)
            record.knockedOut = false; // told to move otherwise: it gets up
        record.asked = settings->mode;
        const RagdollMode mode = record.knockedOut ? RagdollMode::Limp : settings->mode;
        if (mode != record.mode)
        {
            // Kinematic while it only follows the animation, else simulated; motors only when powered.
            // Powered, the animation holds it up: a motor only stiffens its own two bodies, so under
            // gravity a chain (the spine and all it carries) would droop off the pose.
            const JPH::EMotionType motion = mode == RagdollMode::Animated ? JPH::EMotionType::Kinematic : JPH::EMotionType::Dynamic;
            for (const RagdollRecord::Bone& bone : record.bones)
            {
                bodies.SetMotionType(bone.body, motion, JPH::EActivation::Activate);
                bodies.SetGravityFactor(bone.body, mode == RagdollMode::Powered ? 0.0f : 1.0f);
            }
            for (int c = 0; c < static_cast<int>(record.ragdoll->GetConstraintCount()); ++c)
            {
                JPH::TwoBodyConstraint* constraint = record.ragdoll->GetConstraint(c);
                const JPH::EMotorState state = mode == RagdollMode::Powered ? JPH::EMotorState::Position : JPH::EMotorState::Off;
                if (constraint->GetSubType() == JPH::EConstraintSubType::SwingTwist)
                {
                    auto* swing = static_cast<JPH::SwingTwistConstraint*>(constraint);
                    for (JPH::MotorSettings* motor : { &swing->GetSwingMotorSettings(), &swing->GetTwistMotorSettings() })
                    {
                        motor->mSpringSettings.mFrequency = 15.0f;
                        motor->mSpringSettings.mDamping = 1.0f;
                        motor->SetTorqueLimit(std::max(settings->strength, 0.0f));
                    }
                    swing->SetSwingMotorState(state);
                    swing->SetTwistMotorState(state);
                }
                else if (constraint->GetSubType() == JPH::EConstraintSubType::Hinge)
                {
                    auto* hinge = static_cast<JPH::HingeConstraint*>(constraint);
                    hinge->GetMotorSettings().mSpringSettings.mFrequency = 15.0f;
                    hinge->GetMotorSettings().mSpringSettings.mDamping = 1.0f;
                    hinge->GetMotorSettings().SetTorqueLimit(std::max(settings->strength, 0.0f));
                    hinge->SetMotorState(state);
                }
            }
            // Back to the animation: it writes every bone again.
            if (mode == RagdollMode::Animated)
                if (Animator* animator = root.Get<Animator>())
                    animator->Refresh();
            ShowRagdoll(record, mode != RagdollMode::Animated);
            record.mode = mode;
            record.strength = settings->strength;
            record.hasPreviousTarget = false;
        }
        if (mode == RagdollMode::Powered && record.strength != settings->strength)
        {
            record.mode = static_cast<RagdollMode>(255); // set the motors again next step
            continue;
        }
        if (mode == RagdollMode::Limp)
            continue;

        JPH::SkeletonPose pose;
        if (!TargetPose(record, pose))
            continue;
        if (mode == RagdollMode::Animated)
        {
            record.ragdoll->DriveToPoseUsingKinematics(pose, dt);
            continue;
        }
        // Powered: the motors hold the pose (and move as it moves), the hips follow the animation.
        if (record.hasPreviousTarget)
            record.ragdoll->DriveToPoseUsingMotors(record.previousTarget, pose, dt);
        else
            record.ragdoll->DriveToPoseUsingMotors(pose);
        record.previousTarget = pose;
        record.hasPreviousTarget = true;
        const JPH::Mat44& hips = pose.GetJointMatrix(0);
        bodies.MoveKinematic(record.bones[0].body, JPH::RVec3(hips.GetTranslation()), hips.GetQuaternion(), dt);
    }
}

// Something hitting a ragdoll hard enough (not one of its own bones) knocks it out.
void PhysicsWorld::Impl::CheckRagdollImpacts(size_t firstEvent)
{
    for (size_t i = firstEvent; i < events.size(); ++i)
    {
        const ContactEvent& e = events[i];
        if (e.type != ContactEvent::Type::Begin || e.trigger)
            continue;
        const auto a = ragdollBones.find(e.a.RawId()), b = ragdollBones.find(e.b.RawId());
        if (a != ragdollBones.end() && b != ragdollBones.end() && a->second == b->second)
            continue;
        for (const auto& hit : { a, b })
        {
            if (hit == ragdollBones.end())
                continue;
            const auto record = ragdolls.find(hit->second);
            const Ragdoll* settings = scene.FromId(hit->second).IsValid() ? scene.FromId(hit->second).TryGet<Ragdoll>() : nullptr;
            if (record == ragdolls.end() || !settings || settings->limpOnImpact <= 0.0f || settings->mode == RagdollMode::Limp || record->second.knockedOut)
                continue;
            if (std::abs(e.speed) < settings->limpOnImpact)
                continue;
            record->second.knockedOut = true;
            if (settings->knockback > 0.0f)
                KnockBack(record->second, *settings, hit == a ? e.b : e.a, e.point);
        }
    }
}

void PhysicsWorld::Impl::KnockBack(RagdollRecord& record, const Ragdoll& settings, Entity hitter, const glm::vec3& point)
{
    // The hitter's body: a rigid body or another ragdoll's bone.
    JPH::BodyID body = BodyOf(hitter);
    if (const auto bone = ragdollBones.find(hitter.RawId()); body.IsInvalid() && bone != ragdollBones.end())
        for (const RagdollRecord::Bone& b : ragdolls[bone->second].bones)
            if (b.entity == hitter.RawId())
                body = b.body;
    if (body.IsInvalid())
        return;
    JPH::BodyInterface& bodies = system->GetBodyInterface();
    const glm::vec3 velocity = ToGlm(bodies.GetPointVelocity(body, JPH::RVec3(ToJolt(point))));
    const float speed = glm::length(velocity) * settings.knockback;
    if (speed < 1e-3f)
        return;
    const glm::vec3 throwVelocity = glm::normalize(velocity) * speed + glm::vec3(0.0f, speed * settings.knockbackLift, 0.0f);
    const glm::vec3 direction = glm::normalize(throwVelocity);
    for (const RagdollRecord::Bone& bone : record.bones)
    {
        // Only what it lacks: the bodies the hitter already moves keep their speed. Those far from
        // the hit get less, so it tumbles (hit at the legs, it goes over backwards).
        const glm::vec3 v = ToGlm(bodies.GetLinearVelocity(bone.body));
        const float away = glm::distance(ToGlm(JPH::Vec3(bodies.GetCenterOfMassPosition(bone.body))), point);
        const float missing = glm::length(throwVelocity) * std::max(1.0f - 0.25f * away, 0.5f) - glm::dot(v, direction);
        if (missing > 0.0f)
            bodies.SetLinearVelocity(bone.body, ToJolt(v + direction * missing));
    }
}

// Only what the simulation moves is shown: an animated ragdoll's bodies follow its entities.
void PhysicsWorld::Impl::ShowRagdoll(const RagdollRecord& record, bool show)
{
    for (const RagdollRecord::Bone& bone : record.bones)
        if (Entity e = scene.FromId(bone.entity); e.IsValid())
            if (PhysicsBody* shown = e.Get<PhysicsBody>())
                shown->show = show;
}

void PhysicsWorld::SetRagdollMode(Entity entity, RagdollMode mode)
{
    if (Ragdoll* ragdoll = entity.IsValid() ? entity.Get<Ragdoll>() : nullptr)
        ragdoll->mode = mode;
    if (const auto found = m_Impl->ragdolls.find(entity.RawId()); found != m_Impl->ragdolls.end())
        found->second.knockedOut = false;
}

bool PhysicsWorld::AddRagdollImpulse(Entity entity, const glm::vec3& impulse)
{
    // A bone's body, or for the model's root its hips.
    JPH::BodyID body;
    if (const auto found = m_Impl->ragdolls.find(entity.RawId()); found != m_Impl->ragdolls.end() && !found->second.bones.empty())
        body = found->second.bones[0].body;
    else if (const auto bone = m_Impl->ragdollBones.find(entity.RawId()); bone != m_Impl->ragdollBones.end())
        for (const Impl::RagdollRecord::Bone& b : m_Impl->ragdolls[bone->second].bones)
            if (b.entity == entity.RawId())
                body = b.body;
    if (body.IsInvalid())
        return false;
    m_Impl->system->GetBodyInterface().AddImpulse(body, ToJolt(impulse));
    return true;
}

bool PhysicsWorld::GetRagdollInfo(Entity entity, RagdollInfo& info) const
{
    const auto found = m_Impl->ragdolls.find(entity.RawId());
    if (found == m_Impl->ragdolls.end() || !found->second.ragdoll)
        return false;
    const Impl::RagdollRecord& record = found->second;
    info.mode = record.mode;
    info.bones.clear();
    const JPH::BodyInterface& bodies = m_Impl->system->GetBodyInterface();
    std::unordered_set<uint64_t> touching;
    for (const auto& [key, t] : m_Impl->touching)
    {
        const auto a = m_Impl->ragdollBones.find(t.a), b = m_Impl->ragdollBones.find(t.b);
        if (a != m_Impl->ragdollBones.end() && (b == m_Impl->ragdollBones.end() || b->second != a->second))
            touching.insert(t.a);
        if (b != m_Impl->ragdollBones.end() && (a == m_Impl->ragdollBones.end() || a->second != b->second))
            touching.insert(t.b);
    }
    for (const Impl::RagdollRecord::Bone& bone : record.bones)
    {
        RagdollInfo::Bone out;
        out.entity = m_Impl->scene.FromId(bone.entity);
        JPH::RVec3 p;
        JPH::Quat q;
        bodies.GetPositionAndRotation(bone.body, p, q);
        out.position = ToGlm(JPH::Vec3(p));
        const glm::quat r = ToGlm(q);
        out.a = out.position + r * bone.localA;
        out.b = out.position + r * bone.localB;
        out.radius = bone.radius;
        out.touching = touching.count(bone.entity) > 0;
        info.bones.push_back(out);
    }
    return true;
}
