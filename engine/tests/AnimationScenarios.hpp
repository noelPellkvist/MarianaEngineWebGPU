#pragma once
// Animation with known answers on models built in code (TestRig.hpp): root motion walks and turns
// the entity while the hips stay over it, blends walk at the speed they blend to, and a layer on
// top adds no walk. Every scenario steps exactly 1/60 s at a time.
#include "TestCommon.hpp"
#include "TestRig.hpp"

#include <Mariana/Animation/AnimationGraph.hpp>
#include <Mariana/Animation/Animator.hpp>
#include <Mariana/Scene/Transform.hpp>

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <string>

namespace EngineScenarios
{
    namespace detail
    {
        constexpr float kStep = 1.0f / 60.0f;

        inline glm::vec3 PositionOf(Entity e)
        {
            const LocalTRS* t = e.TryGet<LocalTRS>();
            return t ? glm::vec3(t->pos[0], t->pos[1], t->pos[2]) : glm::vec3(0.0f);
        }

        inline float YawDegreesOf(Entity e)
        {
            const LocalTRS* t = e.TryGet<LocalTRS>();
            if (!t)
                return 0.0f;
            const glm::quat q(t->rot_quat[3], t->rot_quat[0], t->rot_quat[1], t->rot_quat[2]);
            return glm::degrees(2.0f * std::atan2(q.y, q.w));
        }

        // A walker with an Animator on its root, playing (or blending, with a graph) its clips.
        struct Walker
        {
            AssetManager2 assets; // outlives the scene, whose components hold handles
            Scene scene;
            Entity root;
            TestRig::Rig rig;
            System animation;

            Walker()
            {
                scene.UpdateComponentRegistry();
                root = scene.Instantiate("Walker");
                rig = TestRig::Make(scene, assets, root, TestRig::Walker(), "test://walker");
                Animator animator;
                animator.skeleton = rig.skeleton;
                animator.rootMotion = true;
                root.Add<Animator>(animator);
                animation = Animator::CreateSystem(scene);
            }

            Animator& Get() { return *root.Get<Animator>(); }
            Entity Hips() const { return rig.bones[1]; }

            // Runs `frames` updates; the hips' farthest from their rest spot (cm, horizontally) and
            // their height range on the way.
            void Run(int frames, float* drift = nullptr, float* low = nullptr, float* high = nullptr)
            {
                for (int i = 0; i < frames; ++i)
                {
                    animation.Run(kStep);
                    const glm::vec3 hips = PositionOf(Hips());
                    if (drift)
                        *drift = std::max(*drift, std::sqrt(hips.x * hips.x + hips.z * hips.z));
                    if (low)
                        *low = std::min(*low, hips.y);
                    if (high)
                        *high = std::max(*high, hips.y);
                }
            }
        };
    }

    // A 1 m/s walk (hips in centimetres under a 0.01-scaled Armature) moves the entity, not the hips.
    inline Result RootMotionWalk(const Log& log)
    {
        using namespace detail;
        Walker w;
        w.Get().clips = { TestRig::WalkClip(w.assets, "Walk", 1.0f) };
        w.Get().Play(0);
        float drift = 0.0f, low = 1e9f, high = -1e9f;
        w.Run(151, &drift, &low, &high); // the first update binds the clips and doesn't move
        const glm::vec3 walked = PositionOf(w.root);
        const glm::vec3 velocity = w.Get().RootVelocity();
        w.Get().speed = 2.0f;
        w.Run(150);
        const float faster = PositionOf(w.root).z - walked.z;
        const bool passed = std::abs(walked.z - 2.5f) < 1e-3f && std::abs(walked.x) < 1e-4f && drift < 1e-3f && low > 94.9f && high > 96.5f &&
                            std::abs(faster - 5.0f) < 2e-3f && std::abs(velocity.z - 1.0f) < 1e-3f;
        return Finish(log, "root motion walk", Format("%.4f m in 2.5 s (x %.5f), hips within %.4f cm of their spot and bobbing %.1f..%.1f cm; ", walked.z, walked.x, drift, low, high) +
                          Format("speed 2: %.4f m more", faster),
                      passed);
    }

    // A clip that turns the hips 90 degrees a second turns the entity instead.
    inline Result RootMotionTurn(const Log& log)
    {
        using namespace detail;
        Walker w;
        w.Get().clips = { TestRig::Clip(w.assets, "Turn", { TestRig::Track("mixamorig:Hips", { 0.0f, 0.5f, 1.0f }, {},
                                                                             { TestRig::Yaw(0.0f), TestRig::Yaw(45.0f), TestRig::Yaw(90.0f) }) }) };
        w.Get().Play(0);
        w.Run(61);
        const float yaw = YawDegreesOf(w.root);
        const float hipsYaw = YawDegreesOf(w.Hips());
        w.Run(60);
        const float more = YawDegreesOf(w.root);
        const bool passed = std::abs(yaw - 90.0f) < 0.05f && std::abs(hipsYaw) < 0.05f && std::abs(std::remainder(more - 180.0f, 360.0f)) < 0.1f;
        return Finish(log, "root motion turn", Format("turned %.3f deg in 1 s (the hips %.3f), %.3f after 2 s", yaw, hipsYaw, more), passed);
    }

    // Events fire once each time the clip passes them (loops wrap; one at 0 fires as the clip starts),
    // a name once per update (two clips in step with the same event step once), and only from clips
    // making up at least a quarter of the pose.
    inline Result AnimationEventsFire(const Log& log)
    {
        using namespace detail;
        std::map<std::string, int> heard;
        const auto listen = [&](Walker& w, int frames) {
            for (int i = 0; i < frames; ++i)
            {
                w.animation.Run(kStep);
                for (const Animator::FiredEvent& e : w.Get().Events())
                    ++heard[e.name + "@" + e.clip];
            }
        };

        Walker a;
        NamedClip walk = TestRig::WalkClip(a.assets, "Walk", 1.0f);
        walk.events = { { 0.0f, "Start" }, { 0.25f, "Left" }, { 0.75f, "Right" }, { 1.0f, "End" } };
        a.Get().clips = { walk };
        a.Get().Play(0);
        listen(a, 150); // 2.5 s
        const bool loops = heard["Start@Walk"] == 3 && heard["Left@Walk"] == 3 && heard["Right@Walk"] == 2 && heard["End@Walk"] == 2;
        const std::string first = Format("over 2.5 s Start %.0f, Left %.0f, Right %.0f, End %.0f", heard["Start@Walk"], heard["Left@Walk"], heard["Right@Walk"], heard["End@Walk"]);

        heard.clear();
        Walker b;
        NamedClip slow = TestRig::WalkClip(b.assets, "Walk", 1.0f), fast = TestRig::WalkClip(b.assets, "Run", 3.0f);
        slow.events = { { 0.5f, "Step" }, { 0.2f, "WalkOnly" } };
        fast.events = { { 0.5f, "Step" } };
        b.Get().clips = { slow, fast };
        AnimationGraph::Builder builder;
        builder.Float("speed", 0.5f);
        builder.Output(builder.Blend1D("speed", { { builder.Clip("Walk", { 1.0f, true, "move" }), 0.0f }, { builder.Clip("Run", { 1.0f, true, "move" }), 1.0f } }));
        b.Get().graph = b.assets.RegisterAsset<AnimationGraph>(builder.Build(), AssetType::AnimationGraph, "Moves", "test://graph/moves");
        listen(b, 120);
        const int steps = heard["Step@Walk"] + heard["Step@Run"], walkOnly = heard["WalkOnly@Walk"];
        heard.clear();
        b.Get().SetFloat("speed", 0.9f); // the walk at 10%: too little to fire
        listen(b, 120);
        const int fastSteps = heard["Step@Run"] + heard["Step@Walk"], quiet = heard["WalkOnly@Walk"];
        const bool passed = loops && steps == 2 && walkOnly == 2 && fastSteps == 2 && quiet == 0;
        return Finish(log, "animation events", first + Format("; blended half and half: Step %.0f, WalkOnly %.0f in 2 s; the walk at 10%%: Step %.0f, WalkOnly %.0f", steps, walkOnly, fastSteps, quiet),
                      passed);
    }

    // A model with an Animator whose graph holds a still clip and puts IK on it.
    struct Puppet
    {
        AssetManager2 assets; // outlives the scene, whose components hold handles
        Scene scene;
        Entity root;
        TestRig::Rig rig;
        System animation;

        Puppet(const std::vector<TestRig::BoneSpec>& bones, const std::shared_ptr<AnimationGraph>& graph)
        {
            scene.UpdateComponentRegistry();
            root = scene.Instantiate("Puppet");
            rig = TestRig::Make(scene, assets, root, bones, "test://puppet");
            Animator animator;
            animator.skeleton = rig.skeleton;
            const glm::vec3 at = bones[0].translation;
            animator.clips = { TestRig::Clip(assets, "Hold", { TestRig::Track(bones[0].name, { 0.0f, 1.0f }, { at, at }) }) };
            animator.graph = assets.RegisterAsset<AnimationGraph>(graph, AssetType::AnimationGraph, "IK", "test://graph/ik");
            root.Add<Animator>(animator);
            animation = Animator::CreateSystem(scene);
        }

        Animator& Get() { return *root.Get<Animator>(); }
        glm::vec3 At(int bone) const { return glm::vec3(WorldMatrix(scene, rig.bones[bone])[3]); }
        glm::vec3 Facing(int bone) const { return glm::normalize(glm::mat3(WorldMatrix(scene, rig.bones[bone])) * glm::vec3(0.0f, 0.0f, 1.0f)); }
        void Run(int frames = 2)
        {
            for (int i = 0; i < frames; ++i)
                animation.Run(detail::kStep);
        }
    };

    inline float DegreesBetween(const glm::vec3& a, const glm::vec3& b)
    {
        return glm::degrees(std::acos(glm::clamp(glm::dot(glm::normalize(a), glm::normalize(b)), -1.0f, 1.0f)));
    }

    // A 0.3 + 0.3 m arm, straight along +X from a shoulder at (0, 1, 0), reaches with Two Bone IK.
    inline Result TwoBoneIKReach(const Log& log)
    {
        using namespace detail;
        const std::vector<TestRig::BoneSpec> arm = { { "Shoulder", -1, glm::vec3(0.0f, 1.0f, 0.0f) }, { "Arm", 0 },
                                                     { "ForeArm", 1, glm::vec3(0.3f, 0.0f, 0.0f) }, { "Hand", 2, glm::vec3(0.3f, 0.0f, 0.0f) } };
        AnimationGraph::Builder b;
        b.Vector("target");
        b.Vector("pole", glm::vec3(0.0f, 1.0f, -1.0f));
        b.Float("ik", 1.0f);
        b.Output(b.TwoBoneIK(b.Clip("Hold"), "Hand", "target", "pole", b.Parameter("ik")));
        Puppet p(arm, b.Build());
        const glm::vec3 shoulder(0.0f, 1.0f, 0.0f);

        // In reach (0.4 m away): the hand on the target, the elbow 0.3 m from both and on the pole's side.
        const glm::vec3 target = shoulder + glm::vec3(0.2f, 0.0f, 0.34641f);
        p.Get().SetVector("target", target);
        p.Run();
        const glm::vec3 hand = p.At(3), elbow = p.At(2);
        const float miss = glm::length(hand - target);
        const glm::vec3 along = glm::normalize(target - shoulder);
        glm::vec3 elbowSide = elbow - shoulder, poleSide = glm::vec3(0.0f, 1.0f, -1.0f) - shoulder;
        elbowSide -= along * glm::dot(elbowSide, along);
        poleSide -= along * glm::dot(poleSide, along);
        const bool reached = miss < 1e-4f && std::abs(glm::length(elbow - shoulder) - 0.3f) < 1e-4f && std::abs(glm::length(hand - elbow) - 0.3f) < 1e-4f &&
                             glm::dot(elbowSide, poleSide) > 0.0f;

        // Half the weight turns the upper arm half as far.
        const glm::vec3 upperFull = glm::normalize(elbow - shoulder);
        p.Get().SetFloat("ik", 0.5f);
        p.Run();
        const float half = DegreesBetween(glm::vec3(1.0f, 0.0f, 0.0f), p.At(2) - shoulder), full = DegreesBetween(glm::vec3(1.0f, 0.0f, 0.0f), upperFull);

        // Out of reach (1 m away): straight at it.
        p.Get().SetFloat("ik", 1.0f);
        p.Get().SetVector("target", shoulder + glm::vec3(0.0f, 0.0f, 1.0f));
        p.Run();
        const glm::vec3 straight = p.At(3) - shoulder;
        const float off = DegreesBetween(straight, glm::vec3(0.0f, 0.0f, 1.0f));
        const bool passed = reached && std::abs(half - 0.5f * full) < 0.5f && off < 0.05f && std::abs(glm::length(straight) - 0.6f) < 1e-3f;
        return Finish(log, "two bone IK", Format("the hand %.5f m off the target, the elbow %.4f / %.4f m from shoulder and hand; at half weight the arm turned %.2f of %.2f deg; ",
                                                 miss, glm::length(elbow - shoulder), glm::length(hand - elbow), half, full) +
                          Format("out of reach it points %.3f deg off, %.4f m long", off, glm::length(straight)),
                      passed);
    }

    // A head on a neck and spine turns to a target with Look At: exactly with the chain sharing it,
    // and no further than its limit.
    inline Result LookAtTurn(const Log& log)
    {
        using namespace detail;
        const std::vector<TestRig::BoneSpec> body = { { "Spine", -1, glm::vec3(0.0f, 1.0f, 0.0f) }, { "Neck", 0, glm::vec3(0.0f, 0.3f, 0.0f) },
                                                      { "Head", 1, glm::vec3(0.0f, 0.2f, 0.0f) } };
        AnimationGraph::Builder b;
        b.Vector("look");
        b.Output(b.LookAt(b.Clip("Hold"), "Head", "look", 3, 80.0f));
        Puppet p(body, b.Build());

        // 45 degrees to the side: the head faces it, the spine taking about a third of the turn.
        const glm::vec3 target(2.0f, 1.5f, 2.0f);
        p.Get().SetVector("look", target);
        p.Run();
        const float facing = DegreesBetween(p.Facing(2), target - p.At(2));
        const float spine = DegreesBetween(p.Facing(0), glm::vec3(0.0f, 0.0f, 1.0f));

        // Behind it: no more than 80 degrees.
        p.Get().SetVector("look", glm::vec3(-0.5f, 1.5f, -2.0f));
        p.Run();
        const float limited = DegreesBetween(p.Facing(2), glm::vec3(0.0f, 0.0f, 1.0f));
        const bool passed = facing < 0.5f && spine > 10.0f && spine < 20.0f && std::abs(limited - 80.0f) < 0.5f;
        return Finish(log, "look at", Format("faces the target %.3f deg off (the spine turned %.1f deg of about 45); one behind it: turned %.2f deg (limit 80)", facing, spine, limited),
                      passed);
    }

    // A Blend1D half way between a 1 and a 3 m/s walk walks at 2; a layer moving the hips sideways on
    // top of it adds no walk.
    inline Result RootMotionBlend(const Log& log)
    {
        using namespace detail;
        Walker w;
        w.Get().clips = { TestRig::WalkClip(w.assets, "Walk1", 1.0f), TestRig::WalkClip(w.assets, "Walk3", 3.0f),
                          TestRig::Clip(w.assets, "Sway", { TestRig::Track("mixamorig:Hips", { 0.0f, 1.0f }, { glm::vec3(0.0f, 95.0f, 0.0f), glm::vec3(100.0f, 95.0f, 0.0f) }) }) };
        AnimationGraph::Builder b;
        b.Float("speed", 0.5f);
        const int walk = b.Blend1D("speed", { { b.Clip("Walk1"), 0.0f }, { b.Clip("Walk3"), 1.0f } });
        b.Output(b.Layer(walk, b.Clip("Sway"), BoneMask{}, AnimationWeight{ 1.0f }));
        std::shared_ptr<AnimationGraph> graph = b.Build();
        if (!graph)
            return Finish(log, "root motion blend", "the graph didn't build", false);
        w.Get().graph = w.assets.RegisterAsset<AnimationGraph>(graph, AssetType::AnimationGraph, "Walks", "test://graph/walks");
        w.Run(61);
        const glm::vec3 walked = PositionOf(w.root);
        const bool passed = std::abs(walked.z - 2.0f) < 2e-3f && std::abs(walked.x) < 1e-4f;
        return Finish(log, "root motion blend", Format("%.4f m in 1 s at speed 0.5 between 1 and 3 m/s (sideways %.5f m with a swaying layer on top)", walked.z, walked.x),
                      passed);
    }
}
